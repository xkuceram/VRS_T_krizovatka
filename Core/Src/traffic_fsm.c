#include "traffic_fsm.h"
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Signaly a konfliktna matica
 *
 * Kazdy zo siedmich signalov ma vlastny stav (cervena, zelena, zlta).
 * Novy signal sa zapne, ked vsetky signaly, s ktorymi je v konflikte,
 * svietia nacerveno aspon TRAFFIC_ALL_RED_MS. Bezpecnost krizovatky preto
 * vyplyva priamo z matice nizsie.
 * ------------------------------------------------------------------------- */

enum
{
    S_A = 0,        /* zlava: rovno aj dole */
    S_AS,           /* sipka A_S: z A dole (pri cervenej na A) */
    S_BR,           /* sprava rovno */
    S_BD,           /* sprava dole */
    S_C,            /* zdola: dolava aj doprava */
    S_CS,           /* sipka C_S: z C doprava (pri cervenej na C) */
    S_P,            /* chodci */
    S_COUNT
};

#define BIT(s)      ((uint8_t)(1U << (s)))
#define M_A         BIT(S_A)
#define M_AS        BIT(S_AS)
#define M_BR        BIT(S_BR)
#define M_BD        BIT(S_BD)
#define M_C         BIT(S_C)
#define M_CS        BIT(S_CS)
#define M_P         BIT(S_P)
#define M_ALL       ((uint8_t)(BIT(S_COUNT) - 1U))
#define M_ROAD      (M_A | M_BR | M_BD | M_C)   /* bezne cestne semafory - koncia zltou */

/*
 * Konfliktna matica: s cim sa pohyb krizuje alebo zlieva do jedneho pruhu.
 *   A   (rovno aj dole)   x B_D (krizuje), C (krizuje / zlieva), C_S (zlieva), P (odbocenie dole)
 *   A_S (z A dole)        x B_D (rovnaky vyjazdovy pruh), P
 *   B_R (rovno dolava)    x C (zlievanie pri odboceni z C dolava)
 *   B_D (dole)            x A, A_S, C, P
 *   C   (dolava aj doprava) x A, B_R, B_D, P
 *   C_S (doprava)         x A, P
 *   P   (priechod cez dolnu vetvu) x A, A_S, B_D, C, C_S
 */
static const uint8_t conflicts[S_COUNT] =
{
    [S_A]  = M_BD | M_C | M_CS | M_P,
    [S_AS] = M_BD | M_P,
    [S_BR] = M_C,
    [S_BD] = M_A | M_AS | M_C | M_P,
    [S_C]  = M_A | M_BR | M_BD | M_P,
    [S_CS] = M_A | M_P,
    [S_P]  = M_A | M_AS | M_BD | M_C | M_CS
};

/* Sipka svieti iba pri cervenej na svojom beznom semafore (ten isty pohyb,
 * preto bez ochranneho casu - sipka moze naskocit hned po zltej). */
static const uint8_t partner[S_COUNT] =
{
    [S_A] = M_AS, [S_AS] = M_A,
    [S_C] = M_CS, [S_CS] = M_C
};

/* Fazy rezimu Demo */
typedef enum
{
    PH_NONE = 0,
    PH_F1,          /* hlavna cesta: A + B_R */
    PH_F2,          /* vjazd C: C (oba smery) + A_S */
    PH_F3,          /* B + sipka C doprava: B_R + B_D + C_S */
    PH_F4           /* chodci + B rovno: P + B_R */
} Phase;

static const uint8_t phase_mask[] =
{
    [PH_NONE] = 0U,
    [PH_F1] = M_A | M_BR,
    [PH_F2] = M_C | M_AS,
    [PH_F3] = M_BR | M_BD | M_CS,
    [PH_F4] = M_BR | M_P
};

/* Ciel prikazu SET -> signaly */
static const uint8_t target_mask[TRAFFIC_TARGET_COUNT] =
{
    [TRAFFIC_TARGET_A]   = M_A,
    [TRAFFIC_TARGET_A_S] = M_AS,
    [TRAFFIC_TARGET_B_R] = M_BR,
    [TRAFFIC_TARGET_B_D] = M_BD,
    [TRAFFIC_TARGET_C]   = M_C,
    [TRAFFIC_TARGET_C_S] = M_CS,
    [TRAFFIC_TARGET_P]   = M_P,
    [TRAFFIC_TARGET_B]   = M_BR | M_BD
};

/* ---------------------------------------------------------------------------
 * Stav automatu
 * ------------------------------------------------------------------------- */

typedef enum
{
    SIG_RED = 0,    /* cervena alebo zhasnuta sipka */
    SIG_GREEN,      /* zelena alebo svietiaca sipka */
    SIG_YELLOW      /* zlta (iba bezne cestne semafory) */
} SigState;

typedef struct
{
    TrafficMode mode;
    TrafficMode next_mode;
    uint32_t now_ms;                /* cas posledneho Update (pouzivaju aj poziadavky) */

    SigState sig[S_COUNT];
    uint32_t since[S_COUNT];        /* cas poslednej zmeny signalu */

    /* rezim Demo */
    Phase phase;                    /* aktualna (alebo nabiehajuca) faza */
    Phase before_ped;               /* faza pred chodcami (F1 alebo F3) */
    uint8_t target;                 /* signaly faze, ktore sa maju zapnut */
    bool phase_on;                  /* vsetky signaly faze uz svietia */
    uint32_t phase_start_ms;        /* odkedy svietia vsetky signaly faze */

    /* vstupy a poziadavky */
    bool car_c;
    bool car_waiting;               /* auto stoji pred snimacom pri nezelenej C */
    uint32_t car_since_ms;
    bool c_req;
    bool p_req;
} FsmState;

static FsmState fsm;
static TrafficOutputCallback output_callback = NULL;
static TrafficSignals shown;        /* posledny oznameny stav svetiel */
static bool shown_valid = false;

/* ---------------------------------------------------------------------------
 * Pomocne funkcie pre signaly
 * ------------------------------------------------------------------------- */

static uint32_t elapsed(uint32_t since_ms)
{
    return (uint32_t)(fsm.now_ms - since_ms);   /* funguje aj pri preteceni */
}

/* Signaly, ktore nesvietia nacerveno (zelene aj zlte) */
static uint8_t lit_mask(void)
{
    uint8_t m = 0U;
    for (int s = 0; s < S_COUNT; s++)
    {
        if (fsm.sig[s] != SIG_RED)
        {
            m |= BIT(s);
        }
    }
    return m;
}

static uint8_t green_mask(void)
{
    uint8_t m = 0U;
    for (int s = 0; s < S_COUNT; s++)
    {
        if (fsm.sig[s] == SIG_GREEN)
        {
            m |= BIT(s);
        }
    }
    return m;
}

/* Vsetky signaly v maske su cervene aspon ochranny cas */
static bool cleared(uint8_t mask)
{
    for (int s = 0; s < S_COUNT; s++)
    {
        if ((mask & BIT(s)) != 0U &&
            (fsm.sig[s] != SIG_RED || elapsed(fsm.since[s]) < TRAFFIC_ALL_RED_MS))
        {
            return false;
        }
    }
    return true;
}

/* Smie sa signal zapnut? Sam aj konfliktne signaly cervene aspon 1 s, partner (A/A_S, C/C_S) cerveny. */
static bool can_start(int s)
{
    return cleared((uint8_t)(BIT(s) | conflicts[s])) &&
           (lit_mask() & partner[s]) == 0U;
}

static void set_signal(int s, SigState state)
{
    fsm.sig[s] = state;
    fsm.since[s] = fsm.now_ms;
    if (state == SIG_GREEN)
    {
        if (s == S_P)
        {
            fsm.p_req = false;      /* poziadavka chodcov je vybavena */
        }
        if (s == S_C)
        {
            fsm.c_req = false;      /* poziadavka z vjazdu C je vybavena */
            fsm.car_waiting = false;
        }
    }
}

/* Ukonci signal: bezny cestny semafor cez zltu, sipka a chodci hned */
static void end_signal(int s)
{
    if (fsm.sig[s] != SIG_GREEN)
    {
        return;
    }
    set_signal(s, ((M_ROAD & BIT(s)) != 0U) ? SIG_YELLOW : SIG_RED);
}

static void end_signals(uint8_t mask)
{
    for (int s = 0; s < S_COUNT; s++)
    {
        if ((mask & BIT(s)) != 0U)
        {
            end_signal(s);
        }
    }
}

/* Vsetko cervene aspon ochranny cas */
static bool all_clear(void)
{
    return cleared(M_ALL);
}

/* ---------------------------------------------------------------------------
 * Vystup
 * ------------------------------------------------------------------------- */

static TrafficColor color_of(int s)
{
    switch (fsm.sig[s])
    {
        case SIG_GREEN:  return TRAFFIC_COLOR_GREEN;
        case SIG_YELLOW: return TRAFFIC_COLOR_YELLOW;
        default:         return TRAFFIC_COLOR_RED;
    }
}

static TrafficSignals current_signals(void)
{
    TrafficSignals s =
    {
        .a = color_of(S_A),
        .b_r = color_of(S_BR),
        .b_d = color_of(S_BD),
        .c = color_of(S_C),
        .p = color_of(S_P),
        .a_s_on = fsm.sig[S_AS] == SIG_GREEN,
        .c_s_on = fsm.sig[S_CS] == SIG_GREEN
    };
    return s;
}

static bool signals_equal(const TrafficSignals *x, const TrafficSignals *y)
{
    return x->a == y->a && x->b_r == y->b_r && x->b_d == y->b_d && x->c == y->c &&
           x->p == y->p && x->a_s_on == y->a_s_on && x->c_s_on == y->c_s_on;
}

/* Oznami zmenu svetiel cez callback (iba pri skutocnej zmene) */
static void publish(void)
{
    const TrafficSignals s = current_signals();

    if (shown_valid && signals_equal(&s, &shown))
    {
        return;
    }
    shown = s;
    shown_valid = true;
    if (output_callback != NULL)
    {
        output_callback(&shown);
    }
}

/* ---------------------------------------------------------------------------
 * Rezim Demo
 * ------------------------------------------------------------------------- */

/*
 * Zmena fazy: signaly, ktore pokracuju aj v novej faze, svietia dalej.
 * Ostatne skoncia (zlta alebo hned). Nove signaly nabiehaju v step(),
 * ked to dovoli konfliktna matica.
 */
static void start_phase(Phase phase)
{
    const uint8_t mask = phase_mask[phase];

    end_signals((uint8_t)(green_mask() & (uint8_t)~mask));
    fsm.phase = phase;
    fsm.target = mask;
    fsm.phase_on = false;
}

/*
 * Poradie faz:  F1 -> [F4 ak caka chodec] -> [F2 ak caka auto na C] -> F3 -> [F4] -> F1
 * B_R svieti v F1, F3 aj F4, preto pri prechode medzi nimi nezastavi.
 */
static Phase demo_next_phase(void)
{
    switch (fsm.phase)
    {
        case PH_F1:
            if (fsm.p_req)
            {
                fsm.before_ped = PH_F1;
                return PH_F4;
            }
            return fsm.c_req ? PH_F2 : PH_F3;

        case PH_F4:
            if (fsm.before_ped == PH_F1)
            {
                return fsm.c_req ? PH_F2 : PH_F3;
            }
            return PH_F1;

        case PH_F2:
            return PH_F3;

        case PH_F3:
            if (fsm.p_req)
            {
                fsm.before_ped = PH_F3;
                return PH_F4;
            }
            return PH_F1;

        case PH_NONE:
        default:
            return PH_F1;
    }
}

static void demo_step(void)
{
    /* zapnut signaly fazy, ktore uz smu naskocit */
    for (int s = 0; s < S_COUNT; s++)
    {
        if ((fsm.target & BIT(s)) != 0U && fsm.sig[s] == SIG_RED && can_start(s))
        {
            set_signal(s, SIG_GREEN);
        }
    }

    if (!fsm.phase_on)
    {
        if ((green_mask() & fsm.target) == fsm.target)
        {
            fsm.phase_on = true;
            fsm.phase_start_ms = fsm.now_ms;
        }
    }
    else if (elapsed(fsm.phase_start_ms) >= TRAFFIC_GREEN_MS)
    {
        start_phase(demo_next_phase());
    }
}

/* ---------------------------------------------------------------------------
 * Jeden krok automatu v case fsm.now_ms
 * ------------------------------------------------------------------------- */

static void step(void)
{
    /* zlta dobehne a prejde na cervenu */
    for (int s = 0; s < S_COUNT; s++)
    {
        if (fsm.sig[s] == SIG_YELLOW && elapsed(fsm.since[s]) >= TRAFFIC_YELLOW_MS)
        {
            set_signal(s, SIG_RED);
        }
    }

    /* dokoncenie prepnutia rezimu: vsetko cervene aspon ochranny cas */
    if (fsm.next_mode != TRAFFIC_MODE_NONE && all_clear())
    {
        fsm.mode = fsm.next_mode;
        fsm.next_mode = TRAFFIC_MODE_NONE;
        if (fsm.mode == TRAFFIC_MODE_DEMO)
        {
            fsm.phase = PH_NONE;
            start_phase(PH_F1);     /* po navrate do Demo zacne fazou F1 */
        }
    }

    if (fsm.mode == TRAFFIC_MODE_DEMO && fsm.next_mode == TRAFFIC_MODE_NONE)
    {
        demo_step();
    }

    publish();
}

/*
 * Poziadavka z vjazdu C: auto musi stat pred snimacom aspon TRAFFIC_CAR_WAIT_MS
 * pri nezelenej C. Zrusi ju iba odchod auta (prechod snimaca z obsadeneho na
 * volny) pocas svietiacej sipky C_S - auto odbocilo doprava. Ak auto odislo
 * skor a snimac je pri zapnuti sipky uz volny, poziadavka ostava.
 */
static void update_car_request(bool car_c)
{
    const bool car_left = fsm.car_c && !car_c;   /* obsadeny -> volny */

    if (car_left && fsm.c_req && fsm.sig[S_CS] == SIG_GREEN)
    {
        fsm.c_req = false;          /* auto odbocilo doprava po sipke */
    }

    if (!car_c || fsm.sig[S_C] == SIG_GREEN)
    {
        fsm.car_waiting = false;    /* volny vjazd alebo pri zelenej C auta prechadzaju */
    }
    else if (!fsm.car_waiting)
    {
        fsm.car_waiting = true;
        fsm.car_since_ms = fsm.now_ms;
    }
    else if (elapsed(fsm.car_since_ms) >= TRAFFIC_CAR_WAIT_MS)
    {
        fsm.c_req = true;
    }
    fsm.car_c = car_c;
}

/* ---------------------------------------------------------------------------
 * Verejne funkcie
 * ------------------------------------------------------------------------- */

void TrafficFsm_RegisterOutputCallback(TrafficOutputCallback callback)
{
    output_callback = callback;
}

void TrafficFsm_Init(uint32_t now_ms)
{
    fsm = (FsmState){
        .mode = TRAFFIC_MODE_DEMO,
        .next_mode = TRAFFIC_MODE_NONE,
        .now_ms = now_ms,
        .phase = PH_NONE,
        .before_ped = PH_F1
    };
    for (int s = 0; s < S_COUNT; s++)
    {
        fsm.sig[s] = SIG_RED;
        fsm.since[s] = now_ms;      /* prva zelena az po 1 s spolocnej cervenej */
    }
    start_phase(PH_F1);
    shown_valid = false;
    publish();                      /* spolocna cervena po inicializacii */
}

void TrafficFsm_Update(uint32_t now_ms, bool car_c, bool ped_pressed)
{
    fsm.now_ms = now_ms;

    /* stlacenie pocas zelenej pre chodcov patri do prebiehajucej fazy */
    if (ped_pressed && fsm.sig[S_P] != SIG_GREEN)
    {
        fsm.p_req = true;
    }
    update_car_request(car_c);

    step();
}

TrafficResult TrafficFsm_RequestMode(TrafficMode mode)
{
    if (mode != TRAFFIC_MODE_DEMO && mode != TRAFFIC_MODE_CONTROL)
    {
        return TRAFFIC_RESULT_ERR_PARAM;
    }

    if (fsm.next_mode != TRAFFIC_MODE_NONE)
    {
        /* prebieha prepinanie: rovnaky ciel potvrdime, navrat do opustaneho rezimu nie */
        return (mode == fsm.next_mode) ? TRAFFIC_RESULT_OK : TRAFFIC_RESULT_ERR_BUSY;
    }

    if (mode == fsm.mode)
    {
        return TRAFFIC_RESULT_OK;   /* ustaleny rezim - potvrdenie */
    }

    /* ukoncit vsetky zelene, cielovy rezim sa aktivuje po spolocnej cervenej */
    fsm.next_mode = mode;
    end_signals(M_ALL);
    fsm.phase = PH_NONE;
    fsm.target = 0U;
    fsm.phase_on = false;
    step();                         /* ak uz je vsetko cervene aspon 1 s, prepne hned */
    return TRAFFIC_RESULT_OK;
}

TrafficResult TrafficFsm_RequestSignal(TrafficTarget target, TrafficColor color)
{
    if ((unsigned int)target >= (unsigned int)TRAFFIC_TARGET_COUNT ||
        (unsigned int)color > (unsigned int)TRAFFIC_COLOR_GREEN)
    {
        return TRAFFIC_RESULT_ERR_PARAM;
    }
    if (fsm.next_mode != TRAFFIC_MODE_NONE)
    {
        return TRAFFIC_RESULT_ERR_BUSY;
    }
    if (fsm.mode != TRAFFIC_MODE_CONTROL)
    {
        return TRAFFIC_RESULT_ERR_MODE;
    }

    const uint8_t mask = target_mask[target];
    const uint8_t green = green_mask();

    switch (color)
    {
        case TRAFFIC_COLOR_GREEN:
            if ((green & mask) == mask)
            {
                return TRAFFIC_RESULT_OK;               /* uz svieti - potvrdenie */
            }
            /* kazdy novy signal musi byt podla matice volny (aj voci zltym) */
            for (int s = 0; s < S_COUNT; s++)
            {
                if ((mask & BIT(s)) != 0U && fsm.sig[s] != SIG_GREEN && !can_start(s))
                {
                    return TRAFFIC_RESULT_ERR_CONFLICT;
                }
            }
            for (int s = 0; s < S_COUNT; s++)
            {
                if ((mask & BIT(s)) != 0U && fsm.sig[s] != SIG_GREEN)
                {
                    set_signal(s, SIG_GREEN);
                }
            }
            break;

        case TRAFFIC_COLOR_YELLOW:
            if ((green & mask & M_ROAD) == 0U)
            {
                return TRAFFIC_RESULT_ERR_STATE;        /* ciel nema zeleny bezny cestny semafor */
            }
            end_signals(mask);
            break;

        case TRAFFIC_COLOR_RED:
        default:
            end_signals(mask);                          /* pri zltej alebo cervenej bez zmeny */
            break;
    }

    step();
    return TRAFFIC_RESULT_OK;
}

void TrafficFsm_GetStatus(TrafficFsmStatus *status)
{
    if (status == NULL)
    {
        return;
    }
    status->mode = fsm.mode;
    status->next_mode = fsm.next_mode;
    status->ready = fsm.mode == TRAFFIC_MODE_CONTROL &&
                    fsm.next_mode == TRAFFIC_MODE_NONE && all_clear();
    status->signals = current_signals();
    status->car_c = fsm.car_c;
    status->c_req = fsm.c_req;
    status->p_req = fsm.p_req;
}
