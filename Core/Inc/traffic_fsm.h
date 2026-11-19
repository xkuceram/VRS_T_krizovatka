#ifndef TRAFFIC_FSM_H
#define TRAFFIC_FSM_H

/*
 * Stavovy automat T-krizovatky (vetva feature/traffic-fsm).
 *
 * Automat nepouziva HAL ani blokujuce cakanie. Aktualny cas v milisekundach
 * (HAL_GetTick()) a vyfiltrovane vstupy (obsadenost vjazdu C, stlacenie
 * tlacidla chodcov) dostava ako parametre, preto sa da cely otestovat na PC
 * so simulovanym casom (tests/test_traffic_fsm.c).
 *
 * Automat nevola ovladac GPIO priamo. Kazdu zmenu svetiel oznami cez
 * zaregistrovanu callback funkciu, ktora dostane stav vsetkych siedmich
 * signalov (TrafficSignals).
 *
 * Kazdy signal ma vlastny stav. Novy signal sa zapne az vtedy, ked vsetky
 * signaly, s ktorymi je v konflikte (konfliktna matica v traffic_fsm.c),
 * svietia nacerveno aspon TRAFFIC_ALL_RED_MS. Signal, ktory pokracuje aj
 * v dalsej faze, pri zmene fazy nezhasne.
 *
 * Pouzitie v hlavnej slucke:
 *   TrafficFsm_RegisterOutputCallback(apply_signals);  // pred inicializaciou
 *   TrafficFsm_Init(HAL_GetTick());
 *   while (1)
 *   {
 *       uint32_t now = HAL_GetTick();
 *       TrafficInputs_Update(now);
 *       TrafficFsm_Update(now, TrafficInputs_IsCarPresent(),
 *                         TrafficInputs_TakePedestrianPress());
 *       ... prikazy z USART -> TrafficFsm_RequestMode / TrafficFsm_RequestSignal
 *   }
 *
 * Vsetky funkcie sa volaju z hlavnej slucky (nie z obsluhy prerusenia),
 * preto stav automatu nepotrebuje "volatile".
 */

#include <stdbool.h>
#include <stdint.h>
#include "traffic_phase.h"   /* TrafficPhase - stare rozhranie, pouziva ho este traffic_gpio.h */

/* Casovanie [ms] */
#define TRAFFIC_GREEN_MS        5000U   /* zelena v rezime Demo (cestna faza aj chodci) */
#define TRAFFIC_YELLOW_MS       2000U   /* cestna zlta */
#define TRAFFIC_ALL_RED_MS      1000U   /* ochranny cas: konfliktny signal musi byt cerveny aspon tolko */
#define TRAFFIC_CAR_WAIT_MS     5000U   /* auto musi na C stat tolko, aby sa zapisala poziadavka na fazu C */

/* Rezim automatu */
typedef enum
{
    TRAFFIC_MODE_DEMO = 0,
    TRAFFIC_MODE_CONTROL,
    TRAFFIC_MODE_NONE                   /* "ziadny cielovy rezim" (NEXT=-) */
} TrafficMode;

/* Farba bezneho semafora (A, B_R, B_D, C, P). Pre P iba RED a GREEN.
 * A a C pri zelenej pustaju oba smery. Sipky A_S a C_S svietia iba pri cervenej
 * na prislusnom beznom semafore. */
typedef enum
{
    TRAFFIC_COLOR_RED = 0,
    TRAFFIC_COLOR_YELLOW,
    TRAFFIC_COLOR_GREEN
} TrafficColor;

/* Ciel prikazu SET - rovnake poradie ako SerialTarget v traffic_serial.h */
typedef enum
{
    TRAFFIC_TARGET_A = 0,
    TRAFFIC_TARGET_A_S,
    TRAFFIC_TARGET_B_R,
    TRAFFIC_TARGET_B_D,
    TRAFFIC_TARGET_C,
    TRAFFIC_TARGET_C_S,
    TRAFFIC_TARGET_P,
    TRAFFIC_TARGET_B,                   /* oba semafory sprava (B_R aj B_D) */
    TRAFFIC_TARGET_COUNT
} TrafficTarget;

/* Vysledok poziadavky - zodpoveda odpovediam protokolu */
typedef enum
{
    TRAFFIC_RESULT_OK = 0,
    TRAFFIC_RESULT_ERR_BUSY,            /* prebieha prepinanie rezimu */
    TRAFFIC_RESULT_ERR_MODE,            /* SET v rezime Demo */
    TRAFFIC_RESULT_ERR_STATE,           /* zlta pre ciel bez zeleneho bezneho cestneho semafora */
    TRAFFIC_RESULT_ERR_CONFLICT,        /* nova zelena v konflikte so svietiacim signalom alebo pocas ochranneho casu */
    TRAFFIC_RESULT_ERR_PARAM            /* neplatny parameter (USART ho zachyti skor ako ERR SYNTAX) */
} TrafficResult;

/* Stav vsetkych signalov krizovatky - vystup pre ovladac GPIO */
typedef struct
{
    TrafficColor a;
    TrafficColor b_r;
    TrafficColor b_d;
    TrafficColor c;
    TrafficColor p;
    bool a_s_on;                        /* zelena sipka A_S */
    bool c_s_on;                        /* zelena sipka C_S */
} TrafficSignals;

/* Udaje pre prikaz STATUS (farby su logicke, pocas zltej YELLOW) */
typedef struct
{
    TrafficMode mode;                   /* aktivny rezim */
    TrafficMode next_mode;              /* cielovy rezim pocas prepinania, inak TRAFFIC_MODE_NONE */
    bool ready;                         /* Control: vsetko cervene aspon 1 s, prijme lubovolnu zelenu */
    TrafficSignals signals;             /* aktualne svetla */
    bool car_c;                         /* auto pred snimacom C (posledna hodnota z Update) */
    bool c_req;                         /* auto stalo na C aspon 5 s, caka faza C */
    bool p_req;                         /* caka poziadavka chodcov */
} TrafficFsmStatus;

/* Callback volany pri kazdej zmene svetiel */
typedef void (*TrafficOutputCallback)(const TrafficSignals *signals);

/* Registracia callbacku. Volat pred TrafficFsm_Init. NULL = bez callbacku. */
void TrafficFsm_RegisterOutputCallback(TrafficOutputCallback callback);

/* Inicializacia: spolocna cervena, rezim Demo, po 1 s zacne faza F1. Vymaze poziadavky C aj chodcov. */
void TrafficFsm_Init(uint32_t now_ms);

/*
 * Volat v kazdom prechode hlavnej slucky.
 *   car_c       - vyfiltrovana obsadenost vjazdu C
 *   ped_pressed - jednorazova udalost "tlacidlo stlacene" (TrafficInputs_TakePedestrianPress)
 */
void TrafficFsm_Update(uint32_t now_ms, bool car_c, bool ped_pressed);

/* Poziadavky z komunikacie. Pouzivaju cas posledneho TrafficFsm_Update. */
TrafficResult TrafficFsm_RequestMode(TrafficMode mode);                         /* MODE DEMO / MODE CONTROL */
TrafficResult TrafficFsm_RequestSignal(TrafficTarget target, TrafficColor color); /* SET <ciel> <farba> */

/* Aktualny stav pre STATUS */
void TrafficFsm_GetStatus(TrafficFsmStatus *status);

#endif
