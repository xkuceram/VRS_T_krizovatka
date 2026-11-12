#include "traffic_fsm.h"

/*
 * Stavovy automat T-krizovatky (tabulka stavov a prechodov je v docs/traffic_fsm.md).
 *
 * Stav automatu tvori:
 *   - aktivny rezim (Demo / Control) a pripadny cielovy rezim pocas prepinania,
 *   - aktualna faza (TrafficPhase) a cas vstupu do nej,
 *   - v rezime Demo index kroku v cykle.
 *
 * Cas fazy sa meria od skutocneho vstupu do fazy, takze zlta aj spolocna cervena
 * dostanu vzdy cely interval, aj ked predchadzajuca faza skoncila s oneskorenim.
 * Rozdiel casov (uint32_t) funguje spravne aj pri preteceni milisekundoveho casu.
 */

/* Jeden krok cyklu Demo */
typedef struct
{
    TrafficPhase phase;
    uint32_t duration_ms;
} DemoStep;

/* Cyklus Demo podla tabulky v README (11 krokov, spolu 30 s) */
static const DemoStep demo_cycle[] =
{
    {TRAFFIC_PHASE_A_GREEN,  TRAFFIC_GREEN_MS},     /*  1 */
    {TRAFFIC_PHASE_A_YELLOW, TRAFFIC_YELLOW_MS},    /*  2 */
    {TRAFFIC_PHASE_ALL_RED,  TRAFFIC_ALL_RED_MS},   /*  3 */
    {TRAFFIC_PHASE_B_GREEN,  TRAFFIC_GREEN_MS},     /*  4 */
    {TRAFFIC_PHASE_B_YELLOW, TRAFFIC_YELLOW_MS},    /*  5 */
    {TRAFFIC_PHASE_ALL_RED,  TRAFFIC_ALL_RED_MS},   /*  6 */
    {TRAFFIC_PHASE_C_GREEN,  TRAFFIC_GREEN_MS},     /*  7 */
    {TRAFFIC_PHASE_C_YELLOW, TRAFFIC_YELLOW_MS},    /*  8 */
    {TRAFFIC_PHASE_ALL_RED,  TRAFFIC_ALL_RED_MS},   /*  9 */
    {TRAFFIC_PHASE_P_GREEN,  TRAFFIC_GREEN_MS},     /* 10 */
    {TRAFFIC_PHASE_ALL_RED,  TRAFFIC_ALL_RED_MS}    /* 11 */
};

#define DEMO_STEP_COUNT     (sizeof(demo_cycle) / sizeof(demo_cycle[0]))
#define DEMO_LAST_STEP      (DEMO_STEP_COUNT - 1U)  /* spolocna cervena pred krokom 1 */

/* Stav automatu */
static TrafficMode active_mode = TRAFFIC_MODE_DEMO;
static TrafficMode next_mode = TRAFFIC_MODE_NONE;
static TrafficPhase phase = TRAFFIC_PHASE_ALL_RED;
static uint32_t phase_start_ms = 0;
static uint32_t demo_step = DEMO_LAST_STEP;

/* Callback pre zmenu fazy (aplikacna vrstva -> ovladac GPIO) */
static TrafficPhaseCallback phase_callback = 0;


/* ---------------------------------------------------------------------------- */
/* Pomocne funkcie                                                              */
/* ---------------------------------------------------------------------------- */

/* Oznamenie fazy registrovanej funkcii - kontrola, ci je callback zaregistrovany */
static void notify_phase(void)
{
    if (phase_callback != 0)
    {
        phase_callback(phase);
    }
}

static void enter_phase(TrafficPhase new_phase, uint32_t now_ms)
{
    uint8_t changed = (uint8_t)(new_phase != phase);

    phase = new_phase;
    phase_start_ms = now_ms;

    if (changed)
    {
        notify_phase();
    }
}

static uint32_t phase_elapsed(uint32_t now_ms)
{
    return now_ms - phase_start_ms;
}

static uint8_t is_road_green(TrafficPhase p)
{
    return (p == TRAFFIC_PHASE_A_GREEN) || (p == TRAFFIC_PHASE_B_GREEN) || (p == TRAFFIC_PHASE_C_GREEN);
}

static uint8_t is_road_yellow(TrafficPhase p)
{
    return (p == TRAFFIC_PHASE_A_YELLOW) || (p == TRAFFIC_PHASE_B_YELLOW) || (p == TRAFFIC_PHASE_C_YELLOW);
}

/* Zelena cestna faza -> nasledujuca zlta faza toho isteho semaforu */
static TrafficPhase yellow_after(TrafficPhase green)
{
    switch (green)
    {
        case TRAFFIC_PHASE_A_GREEN: return TRAFFIC_PHASE_A_YELLOW;
        case TRAFFIC_PHASE_B_GREEN: return TRAFFIC_PHASE_B_YELLOW;
        case TRAFFIC_PHASE_C_GREEN: return TRAFFIC_PHASE_C_YELLOW;
        default:                    return TRAFFIC_PHASE_ALL_RED;
    }
}

/* Zelena faza zvoleneho semaforu */
static TrafficPhase green_phase_of(TrafficLight light)
{
    switch (light)
    {
        case TRAFFIC_LIGHT_A: return TRAFFIC_PHASE_A_GREEN;
        case TRAFFIC_LIGHT_B: return TRAFFIC_PHASE_B_GREEN;
        case TRAFFIC_LIGHT_C: return TRAFFIC_PHASE_C_GREEN;
        default:              return TRAFFIC_PHASE_P_GREEN;
    }
}

/* Ochranny interval spolocnej cervenej uplynul */
static uint8_t all_red_guard_done(uint32_t now_ms)
{
    return (phase == TRAFFIC_PHASE_ALL_RED) && (phase_elapsed(now_ms) >= TRAFFIC_ALL_RED_MS);
}

/* Aktivacia cieloveho rezimu (vola sa az po uplynuti spolocnej cervenej) */
static void activate_next_mode(uint32_t now_ms)
{
    active_mode = next_mode;
    next_mode = TRAFFIC_MODE_NONE;

    if (active_mode == TRAFFIC_MODE_DEMO)
    {
        /* Demo zacina fazou A (krok 1) */
        demo_step = 0U;
        enter_phase(demo_cycle[0].phase, now_ms);
    }
    /* Control: semafory zostanu na spolocnej cervenej a automat caka na prikaz */
}

/*
 * Ukoncenie aktualnej fazy pri poziadavke na zmenu rezimu:
 * cestna zelena -> zlta, zelena pre chodcov -> spolocna cervena,
 * zlta a spolocna cervena dobehnu.
 */
static void start_phase_termination(uint32_t now_ms)
{
    if (is_road_green(phase))
    {
        enter_phase(yellow_after(phase), now_ms);
    }
    else if (phase == TRAFFIC_PHASE_P_GREEN)
    {
        enter_phase(TRAFFIC_PHASE_ALL_RED, now_ms);
    }
}


/* ---------------------------------------------------------------------------- */
/* Verejne funkcie                                                              */
/* ---------------------------------------------------------------------------- */

void TrafficFsm_RegisterPhaseCallback(TrafficPhaseCallback callback)
{
    phase_callback = callback;
}

void TrafficFsm_Init(uint32_t now_ms)
{
    /* po inicializacii spolocna cervena, Demo zacne prvou zelenou az po 1 s */
    active_mode = TRAFFIC_MODE_DEMO;
    next_mode = TRAFFIC_MODE_NONE;
    demo_step = DEMO_LAST_STEP;
    phase = TRAFFIC_PHASE_ALL_RED;
    phase_start_ms = now_ms;
    notify_phase();                     /* pociatocny stav sa oznami vzdy */
}

TrafficPhase TrafficFsm_Update(uint32_t now_ms)
{
    uint32_t elapsed = phase_elapsed(now_ms);

    if (next_mode != TRAFFIC_MODE_NONE)
    {
        /* prebieha prepinanie rezimu: zlta -> spolocna cervena -> cielovy rezim */
        if (is_road_yellow(phase) && elapsed >= TRAFFIC_YELLOW_MS)
        {
            enter_phase(TRAFFIC_PHASE_ALL_RED, now_ms);
        }
        else if (all_red_guard_done(now_ms))
        {
            activate_next_mode(now_ms);
        }
    }
    else if (active_mode == TRAFFIC_MODE_DEMO)
    {
        /* cyklus Demo - po uplynuti trvania kroku prechod na dalsi krok */
        if (elapsed >= demo_cycle[demo_step].duration_ms)
        {
            demo_step = (demo_step + 1U) % DEMO_STEP_COUNT;
            enter_phase(demo_cycle[demo_step].phase, now_ms);
        }
    }
    else
    {
        /* Control - zlta dobehne automaticky, zelena trva do prikazu */
        if (is_road_yellow(phase) && elapsed >= TRAFFIC_YELLOW_MS)
        {
            enter_phase(TRAFFIC_PHASE_ALL_RED, now_ms);
        }
    }

    return phase;
}

TrafficResult TrafficFsm_RequestMode(TrafficMode mode, uint32_t now_ms)
{
    if (mode != TRAFFIC_MODE_DEMO && mode != TRAFFIC_MODE_CONTROL)
    {
        return TRAFFIC_RESULT_ERR_PARAM;
    }

    /* aktualizacia casovania, aby sa poziadavka vyhodnotila voci aktualnej faze */
    (void)TrafficFsm_Update(now_ms);

    if (next_mode != TRAFFIC_MODE_NONE)
    {
        /* pocas prepinania: zopakovanie ciela = OK, navrat do opustaneho rezimu = BUSY */
        return (mode == next_mode) ? TRAFFIC_RESULT_OK : TRAFFIC_RESULT_ERR_BUSY;
    }

    if (mode == active_mode)
    {
        /* rezim uz je aktivny - len potvrdenie */
        return TRAFFIC_RESULT_OK;
    }

    /* zaciatok prepinania */
    next_mode = mode;
    start_phase_termination(now_ms);

    /* ak spolocna cervena uz uplynula, rezim sa prepne ihned */
    if (all_red_guard_done(now_ms))
    {
        activate_next_mode(now_ms);
    }

    return TRAFFIC_RESULT_OK;
}

TrafficResult TrafficFsm_RequestColor(TrafficLight light, TrafficColor color, uint32_t now_ms)
{
    TrafficPhase green;

    if ((unsigned int)light >= (unsigned int)TRAFFIC_LIGHT_COUNT ||
        (color != TRAFFIC_COLOR_RED && color != TRAFFIC_COLOR_YELLOW && color != TRAFFIC_COLOR_GREEN))
    {
        return TRAFFIC_RESULT_ERR_PARAM;
    }

    (void)TrafficFsm_Update(now_ms);

    /* SET je mozny len v aktivnom rezime Control po dokonceni prepnutia */
    if (next_mode != TRAFFIC_MODE_NONE)
    {
        return TRAFFIC_RESULT_ERR_BUSY;
    }
    if (active_mode != TRAFFIC_MODE_CONTROL)
    {
        return TRAFFIC_RESULT_ERR_MODE;
    }

    green = green_phase_of(light);

    switch (color)
    {
        case TRAFFIC_COLOR_GREEN:
            if (phase == green)
            {
                return TRAFFIC_RESULT_OK;                   /* uz svieti nazeleno */
            }
            if (!all_red_guard_done(now_ms))
            {
                return TRAFFIC_RESULT_ERR_CONFLICT;         /* ina zelena, zlta alebo bezi spolocna cervena */
            }
            enter_phase(green, now_ms);
            return TRAFFIC_RESULT_OK;

        case TRAFFIC_COLOR_YELLOW:
            if (light == TRAFFIC_LIGHT_P || phase != green)
            {
                return TRAFFIC_RESULT_ERR_STATE;            /* P nema zltu, alebo semafor nema zelenu */
            }
            enter_phase(yellow_after(green), now_ms);
            return TRAFFIC_RESULT_OK;

        case TRAFFIC_COLOR_RED:
        default:
            if (phase == green)
            {
                /* cestna zelena konci zltou, zelena pre chodcov priamo spolocnou cervenou */
                enter_phase((light == TRAFFIC_LIGHT_P) ? TRAFFIC_PHASE_ALL_RED : yellow_after(green), now_ms);
            }
            /* zlta dobehne sama, cerveny semafor len potvrdi svoj stav */
            return TRAFFIC_RESULT_OK;
    }
}

TrafficPhase TrafficFsm_GetPhase(void)
{
    return phase;
}

void TrafficFsm_GetStatus(uint32_t now_ms, TrafficFsmStatus *status)
{
    if (status == 0)
    {
        return;
    }

    (void)TrafficFsm_Update(now_ms);

    status->mode = active_mode;
    status->next_mode = next_mode;
    status->phase = phase;
    status->ready = (uint8_t)((active_mode == TRAFFIC_MODE_CONTROL) &&
                              (next_mode == TRAFFIC_MODE_NONE) &&
                              all_red_guard_done(now_ms));
}

TrafficColor TrafficFsm_PhaseColor(TrafficPhase p, TrafficLight light)
{
    switch (light)
    {
        case TRAFFIC_LIGHT_A:
            if (p == TRAFFIC_PHASE_A_GREEN)  return TRAFFIC_COLOR_GREEN;
            if (p == TRAFFIC_PHASE_A_YELLOW) return TRAFFIC_COLOR_YELLOW;
            break;
        case TRAFFIC_LIGHT_B:
            if (p == TRAFFIC_PHASE_B_GREEN)  return TRAFFIC_COLOR_GREEN;
            if (p == TRAFFIC_PHASE_B_YELLOW) return TRAFFIC_COLOR_YELLOW;
            break;
        case TRAFFIC_LIGHT_C:
            if (p == TRAFFIC_PHASE_C_GREEN)  return TRAFFIC_COLOR_GREEN;
            if (p == TRAFFIC_PHASE_C_YELLOW) return TRAFFIC_COLOR_YELLOW;
            break;
        case TRAFFIC_LIGHT_P:
            if (p == TRAFFIC_PHASE_P_GREEN)  return TRAFFIC_COLOR_GREEN;
            break;
        default:
            break;
    }
    return TRAFFIC_COLOR_RED;
}
