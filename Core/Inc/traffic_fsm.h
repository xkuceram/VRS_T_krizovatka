#ifndef TRAFFIC_FSM_H
#define TRAFFIC_FSM_H

/*
 * Stavovy automat T-krizovatky.
 *
 * Automat nepouziva HAL ani blokujuce cakanie. Aktualny cas v milisekundach
 * (napr. HAL_GetTick()) dostava ako parameter, preto sa da testovat aj na PC
 * so simulovanym casom. Vystupom je jedna faza typu TrafficPhase, ktoru
 * hlavna slucka posle ovladacu GPIO (TrafficGpio_Apply).
 *
 * Automat nevola ovladac GPIO priamo (oddelenie aplikacnej vrstvy od HW).
 * Zmenu fazy oznamuje cez zaregistrovanu callback funkciu.
 *
 * Pouzitie v hlavnej slucke:
 *   TrafficFsm_RegisterPhaseCallback(TrafficGpio_Apply);   // pred inicializaciou
 *   TrafficFsm_Init(HAL_GetTick());
 *   while (1)
 *   {
 *       TrafficFsm_Update(HAL_GetTick());
 *       ... spracovanie prikazov z USART -> TrafficFsm_RequestMode / TrafficFsm_RequestColor
 *   }
 *
 * Vsetky funkcie automatu sa volaju z hlavnej slucky (nie z obsluhy prerusenia),
 * preto stav automatu nie je zdielany s prerusenim a nepotrebuje "volatile".
 */

#include <stdint.h>
#include "traffic_phase.h"

/* Casovanie faz [ms] */
#define TRAFFIC_GREEN_MS        5000U   /* zelena v rezime Demo (cestna aj pre chodcov) */
#define TRAFFIC_YELLOW_MS       2000U   /* cestna zlta */
#define TRAFFIC_ALL_RED_MS      1000U   /* ochranny interval spolocnej cervenej */

/* Rezim automatu */
typedef enum
{
    TRAFFIC_MODE_DEMO = 0,
    TRAFFIC_MODE_CONTROL,
    TRAFFIC_MODE_NONE                   /* pouziva sa pre "ziadny cielovy rezim" (NEXT=-) */
} TrafficMode;

/* Semafor */
typedef enum
{
    TRAFFIC_LIGHT_A = 0,
    TRAFFIC_LIGHT_B,
    TRAFFIC_LIGHT_C,
    TRAFFIC_LIGHT_P,
    TRAFFIC_LIGHT_COUNT
} TrafficLight;

/* Farba semaforu */
typedef enum
{
    TRAFFIC_COLOR_RED = 0,
    TRAFFIC_COLOR_YELLOW,
    TRAFFIC_COLOR_GREEN
} TrafficColor;

/* Vysledok poziadavky - zodpoveda odpovediam protokolu (OK, ERR BUSY, ERR MODE, ERR STATE, ERR CONFLICT) */
typedef enum
{
    TRAFFIC_RESULT_OK = 0,
    TRAFFIC_RESULT_ERR_BUSY,            /* prebieha prepinanie rezimu */
    TRAFFIC_RESULT_ERR_MODE,            /* SET v aktivnom rezime Demo */
    TRAFFIC_RESULT_ERR_STATE,           /* zlta pre P alebo pre semafor, ktory nema zelenu */
    TRAFFIC_RESULT_ERR_CONFLICT,        /* nova zelena pri inej zelenej, zltej alebo pocas spolocnej cervenej */
    TRAFFIC_RESULT_ERR_PARAM            /* neplatny parameter (semafor, farba, rezim) */
} TrafficResult;

/* Stav automatu pre prikaz STATUS */
typedef struct
{
    TrafficMode mode;                   /* aktivny rezim */
    TrafficMode next_mode;              /* cielovy rezim pocas prepinania, inak TRAFFIC_MODE_NONE */
    uint8_t ready;                      /* 1 = Control caka pri spolocnej cervenej po uplynuti ochranneho intervalu */
    TrafficPhase phase;                 /* aktualna faza automatu */
} TrafficFsmStatus;

/* Typ callback funkcie volanej pri zmene fazy (napr. TrafficGpio_Apply) */
typedef void (*TrafficPhaseCallback)(TrafficPhase phase);

/*
 * Registracia callback funkcie pre zmenu fazy. Registrovat pred TrafficFsm_Init,
 * aby sa uz pociatocna spolocna cervena zobrazila. NULL = bez callbacku.
 */
void TrafficFsm_RegisterPhaseCallback(TrafficPhaseCallback callback);

/* Inicializacia - spolocna cervena, po 1 s zacne rezim Demo zelenou pre A */
void TrafficFsm_Init(uint32_t now_ms);

/* Vyhodnotenie casovania, volat periodicky z hlavnej slucky. Vracia aktualnu fazu. */
TrafficPhase TrafficFsm_Update(uint32_t now_ms);

/* Poziadavky z komunikacie */
TrafficResult TrafficFsm_RequestMode(TrafficMode mode, uint32_t now_ms);                         /* MODE DEMO / MODE CONTROL */
TrafficResult TrafficFsm_RequestColor(TrafficLight light, TrafficColor color, uint32_t now_ms);  /* SET <semafor> <farba> */

/* Aktualny stav */
TrafficPhase TrafficFsm_GetPhase(void);
void TrafficFsm_GetStatus(uint32_t now_ms, TrafficFsmStatus *status);

/* Farba zvoleneho semaforu v danej faze (napr. pre vypis STATUS z faze aplikovanej na GPIO) */
TrafficColor TrafficFsm_PhaseColor(TrafficPhase phase, TrafficLight light);

#endif
