#include "traffic_fsm.h"
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Signaly ako bitova maska
 *
 * Faza = mnozina signalov, ktore svietia nazeleno (bezne semafory) alebo
 * svietia (sipky). Vsetky ostatne su na cervenej alebo zhasnute.
 * ------------------------------------------------------------------------- */

#define SIG_A       (1U << 0)
#define SIG_A_S     (1U << 1)
#define SIG_B_R     (1U << 2)
#define SIG_B_D     (1U << 3)
#define SIG_C       (1U << 4)
#define SIG_C_S     (1U << 5)
#define SIG_P       (1U << 6)

#define SIG_NONE    0U
#define SIG_ARROWS  (SIG_A_S | SIG_C_S)
#define SIG_ROAD    (SIG_A | SIG_B_R | SIG_B_D | SIG_C)   /* bezne cestne semafory (maju zltu) */

/* Fazy rezimu Demo (tabulka v README) */
#define PHASE_A          (SIG_A | SIG_B_R)
#define PHASE_C_LEFT     (SIG_C | SIG_A_S)
#define PHASE_B_WITH_CS  (SIG_B_R | SIG_B_D | SIG_C_S)
#define PHASE_B_FREE     (SIG_B_R | SIG_B_D)
#define PHASE_PED        (SIG_P)

/* Ciel prikazu SET -> signaly */
static const uint8_t target_mask[TRAFFIC_TARGET_COUNT] =
{
    [TRAFFIC_TARGET_A]   = SIG_A,
    [TRAFFIC_TARGET_A_S] = SIG_A_S,
    [TRAFFIC_TARGET_B_R] = SIG_B_R,
    [TRAFFIC_TARGET_B_D] = SIG_B_D,
    [TRAFFIC_TARGET_C]   = SIG_C,
    [TRAFFIC_TARGET_C_S] = SIG_C_S,
    [TRAFFIC_TARGET_P]   = SIG_P,
    [TRAFFIC_TARGET_B]   = SIG_B_R | SIG_B_D
};

/* ---------------------------------------------------------------------------
 * Stav automatu
 * ------------------------------------------------------------------------- */

/* Etapa fazy */
typedef enum
{
    STAGE_ALL_RED = 0,      /* spolocna cervena (ochranny interval) */
    STAGE_GREEN,            /* svieti mnozina green_mask */
    STAGE_YELLOW            /* zlta na yellow_mask, vsetko ostatne cervene */
} Stage;

/* Nasledujuca cestna faza v rezime Demo */
typedef enum
{
    DEMO_NEXT_A = 0,
    DEMO_NEXT_DECIDE,       /* po faze A: rozhodne obsadenost vjazdu C */
    DEMO_NEXT_C_LEFT,       /* obsadeny vjazd C: C dolava, potom B s C_S */
    DEMO_NEXT_B_WITH_CS,
    DEMO_NEXT_B_FREE        /* volny vjazd C */
} DemoNext;

typedef struct
{
    TrafficMode mode;
    TrafficMode next_mode;
    Stage stage;
    uint8_t green_mask;     /* platne v STAGE_GREEN */
    uint8_t yellow_mask;    /* platne v STAGE_YELLOW */
    uint32_t stage_start_ms;
    uint32_t now_ms;        /* cas posledneho Update (pouzivaju poziadavky) */

    DemoNext demo_next;
    bool ped_may_follow;    /* posledna ukoncena faza bola cestna faza Demo -> moze nasledovat P */

    bool car_c;
    bool p_req;
} FsmState;

static FsmState fsm;
static TrafficOutputCallback output_callback = NULL;
static TrafficSignals shown;            /* posledny oznameny stav svetiel */
static bool shown_valid = false;

/* ---------------------------------------------------------------------------
 * Vystup
 * ------------------------------------------------------------------------- */

static TrafficColor road_color(uint8_t sig)
{
    if (fsm.stage == STAGE_GREEN && (fsm.green_mask & sig) != 0U)
    {
        return TRAFFIC_COLOR_GREEN;
    }
    if (fsm.stage == STAGE_YELLOW && (fsm.yellow_mask & sig) != 0U)
    {
        return TRAFFIC_COLOR_YELLOW;
    }
    return TRAFFIC_COLOR_RED;
}

static TrafficSignals current_signals(void)
{
    const bool green = (fsm.stage == STAGE_GREEN);
    TrafficSignals s =
    {
        .a = road_color(SIG_A),
        .b_r = road_color(SIG_B_R),
        .b_d = road_color(SIG_B_D),
        .c = road_color(SIG_C),
        .p = (green && (fsm.green_mask & SIG_P) != 0U) ? TRAFFIC_COLOR_GREEN : TRAFFIC_COLOR_RED,
        .a_s_on = green && (fsm.green_mask & SIG_A_S) != 0U,
        .c_s_on = green && (fsm.green_mask & SIG_C_S) != 0U
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
 * Prechody medzi etapami
 * ------------------------------------------------------------------------- */

static uint32_t stage_elapsed(void)
{
    return (uint32_t)(fsm.now_ms - fsm.stage_start_ms);   /* funguje aj pri preteceni */
}

static void enter_stage(Stage stage)
{
    fsm.stage = stage;
    fsm.stage_start_ms = fsm.now_ms;
}

static void start_green(uint8_t mask)
{
    fsm.green_mask = mask;
    if ((mask & SIG_P) != 0U)
    {
        fsm.p_req = false;              /* poziadavka chodcov je vybavena */
    }
    enter_stage(STAGE_GREEN);
}

/*
 * Ukonci aktivnu zelenu fazu. Sipky zhasnu hned, bezne cestne semafory
 * so zelenou prejdu na zltu. Faza bez nich (iba sipka alebo chodci) prejde
 * rovno na spolocnu cervenu.
 */
static void end_green(void)
{
    const uint8_t road = (uint8_t)(fsm.green_mask & SIG_ROAD);

    if (fsm.mode == TRAFFIC_MODE_DEMO)
    {
        /* chodci mozu prist na rad po kazdej ukoncenej cestnej faze Demo */
        fsm.ped_may_follow = (fsm.green_mask & SIG_P) == 0U;
    }

    if (road != 0U)
    {
        fsm.yellow_mask = road;
        enter_stage(STAGE_YELLOW);
    }
    else
    {
        enter_stage(STAGE_ALL_RED);
    }
    fsm.green_mask = SIG_NONE;
}

static bool guard_elapsed(void)
{
    return fsm.stage == STAGE_ALL_RED && stage_elapsed() >= TRAFFIC_ALL_RED_MS;
}

/* Vyber dalsej zelenej v rezime Demo po skonceni ochrannej cervenej */
static void demo_next_green(void)
{
    if (fsm.demo_next == DEMO_NEXT_DECIDE)
    {
        /* obsadenost C vyhodnotime po faze A a ochrannej sekunde */
        fsm.demo_next = fsm.car_c ? DEMO_NEXT_C_LEFT : DEMO_NEXT_B_FREE;
    }

    if (fsm.p_req && fsm.ped_may_follow)
    {
        /* chodci dostanu zelenu, naplanovana cestna faza (demo_next) pride po nich */
        start_green(PHASE_PED);
        return;
    }

    switch (fsm.demo_next)
    {
        case DEMO_NEXT_C_LEFT:
            fsm.demo_next = DEMO_NEXT_B_WITH_CS;   /* druha faza pride aj pri uvolnenom vjazde */
            start_green(PHASE_C_LEFT);
            break;
        case DEMO_NEXT_B_WITH_CS:
            fsm.demo_next = DEMO_NEXT_A;
            start_green(PHASE_B_WITH_CS);
            break;
        case DEMO_NEXT_B_FREE:
            fsm.demo_next = DEMO_NEXT_A;
            start_green(PHASE_B_FREE);
            break;
        case DEMO_NEXT_A:
        default:
            fsm.demo_next = DEMO_NEXT_DECIDE;
            start_green(PHASE_A);
            break;
    }
}

/* Jeden krok automatu v case fsm.now_ms */
static void step(void)
{
    /* zlta dobehne a prejde na spolocnu cervenu */
    if (fsm.stage == STAGE_YELLOW && stage_elapsed() >= TRAFFIC_YELLOW_MS)
    {
        enter_stage(STAGE_ALL_RED);
    }

    /* dokoncenie prepnutia rezimu po ochrannej cervenej */
    if (fsm.next_mode != TRAFFIC_MODE_NONE && guard_elapsed())
    {
        fsm.mode = fsm.next_mode;
        fsm.next_mode = TRAFFIC_MODE_NONE;
        if (fsm.mode == TRAFFIC_MODE_DEMO)
        {
            fsm.demo_next = DEMO_NEXT_A;   /* po navrate do Demo zacne od fazy A */
            fsm.ped_may_follow = false;
        }
    }

    if (fsm.mode == TRAFFIC_MODE_DEMO && fsm.next_mode == TRAFFIC_MODE_NONE)
    {
        if (fsm.stage == STAGE_GREEN && stage_elapsed() >= TRAFFIC_GREEN_MS)
        {
            end_green();
        }
        else if (guard_elapsed())
        {
            demo_next_green();
        }
    }

    publish();
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
        .stage = STAGE_ALL_RED,
        .stage_start_ms = now_ms,
        .now_ms = now_ms,
        .demo_next = DEMO_NEXT_A,
        .ped_may_follow = false,
        .car_c = false,
        .p_req = false
    };
    shown_valid = false;
    publish();                          /* spolocna cervena po inicializacii */
}

void TrafficFsm_Update(uint32_t now_ms, bool car_c, bool ped_pressed)
{
    fsm.now_ms = now_ms;
    fsm.car_c = car_c;

    /* stlacenie pocas zelenej pre chodcov patri do prebiehajucej fazy */
    if (ped_pressed && !(fsm.stage == STAGE_GREEN && (fsm.green_mask & SIG_P) != 0U))
    {
        fsm.p_req = true;
    }

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
        return TRAFFIC_RESULT_OK;       /* ustaleny rezim - potvrdenie */
    }

    fsm.next_mode = mode;
    if (fsm.stage == STAGE_GREEN)
    {
        end_green();
    }
    step();                             /* ak ochranna sekunda uplynula, prepne hned */
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
    const bool in_green = (fsm.stage == STAGE_GREEN);

    switch (color)
    {
        case TRAFFIC_COLOR_GREEN:
            if (in_green && (fsm.green_mask & mask) == mask)
            {
                return TRAFFIC_RESULT_OK;               /* uz svieti - potvrdenie */
            }
            if (!guard_elapsed())
            {
                return TRAFFIC_RESULT_ERR_CONFLICT;     /* aktivna faza, zlta alebo ochranna cervena */
            }
            start_green(mask);
            break;

        case TRAFFIC_COLOR_YELLOW:
            if (!in_green || (fsm.green_mask & mask & SIG_ROAD) == 0U)
            {
                return TRAFFIC_RESULT_ERR_STATE;
            }
            end_green();
            break;

        case TRAFFIC_COLOR_RED:
        default:
            if (in_green && (fsm.green_mask & mask) != 0U)
            {
                end_green();                            /* ukonci celu aktivnu fazu */
            }
            /* inak potvrdi aktualny stav bez zmeny casovania */
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
                    fsm.next_mode == TRAFFIC_MODE_NONE && guard_elapsed();
    status->signals = current_signals();
    status->car_c = fsm.car_c;
    status->p_req = fsm.p_req;
}
