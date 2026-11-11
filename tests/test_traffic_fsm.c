/*
 * Testy stavoveho automatu T-krizovatky (skusky F1 - F3) so simulovanym casom.
 *
 * Automat dostava cas ako parameter, preto sa da overit na PC bez dosky:
 *   gcc -std=c99 -Wall -Wextra -I../Core/Inc test_traffic_fsm.c ../Core/Src/traffic_fsm.c -o test_traffic_fsm
 *   ./test_traffic_fsm
 * alebo v priecinku tests: make
 */

#include <stdio.h>
#include <stdint.h>
#include "traffic_fsm.h"

static int failures = 0;
static int checks = 0;
static uint32_t now = 0;

static const char *phase_name(TrafficPhase p)
{
    static const char *names[] = {"ALL_RED", "A_GREEN", "A_YELLOW", "B_GREEN", "B_YELLOW",
                                  "C_GREEN", "C_YELLOW", "P_GREEN"};
    return ((unsigned int)p < (unsigned int)TRAFFIC_PHASE_COUNT) ? names[p] : "?";
}

static const char *result_name(TrafficResult r)
{
    static const char *names[] = {"OK", "ERR BUSY", "ERR MODE", "ERR STATE", "ERR CONFLICT", "ERR PARAM"};
    return names[r];
}

static const char *mode_name(TrafficMode m)
{
    return (m == TRAFFIC_MODE_DEMO) ? "DEMO" : (m == TRAFFIC_MODE_CONTROL) ? "CONTROL" : "-";
}

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("  FAIL t=%5u ms: ", (unsigned)now); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void expect_phase(TrafficPhase expected)
{
    TrafficPhase p = TrafficFsm_GetPhase();
    CHECK(p == expected, "faza %s, ocakavana %s", phase_name(p), phase_name(expected));
}

static void expect_result(TrafficResult r, TrafficResult expected, const char *command)
{
    printf("  t=%5u ms  %-16s -> %s\n", (unsigned)now, command, result_name(r));
    CHECK(r == expected, "%s vratil %s, ocakavane %s", command, result_name(r), result_name(expected));
}

static void expect_status(TrafficMode mode, TrafficMode next, uint8_t ready)
{
    TrafficFsmStatus s;
    TrafficFsm_GetStatus(now, &s);
    printf("  t=%5u ms  STATUS           -> MODE=%s NEXT=%s READY=%u A=%c B=%c C=%c P=%c\n",
           (unsigned)now, mode_name(s.mode), mode_name(s.next_mode), s.ready,
           "RYG"[TrafficFsm_PhaseColor(s.phase, TRAFFIC_LIGHT_A)], "RYG"[TrafficFsm_PhaseColor(s.phase, TRAFFIC_LIGHT_B)],
           "RYG"[TrafficFsm_PhaseColor(s.phase, TRAFFIC_LIGHT_C)], "RYG"[TrafficFsm_PhaseColor(s.phase, TRAFFIC_LIGHT_P)]);
    CHECK(s.mode == mode && s.next_mode == next && s.ready == ready,
          "STATUS MODE=%s NEXT=%s READY=%u, ocakavane MODE=%s NEXT=%s READY=%u",
          mode_name(s.mode), mode_name(s.next_mode), s.ready, mode_name(mode), mode_name(next), ready);
}

/* Bezpecnostne pravidlo: v kazdej faze svieti nazeleno/nazlto najviac jeden semafor, ostatne cervena */
static void check_safety(TrafficPhase p)
{
    int not_red = 0;
    for (int l = 0; l < TRAFFIC_LIGHT_COUNT; l++)
    {
        if (TrafficFsm_PhaseColor(p, (TrafficLight)l) != TRAFFIC_COLOR_RED) not_red++;
    }
    CHECK(not_red <= 1, "vo faze %s nesvieti cervena na %d semaforoch", phase_name(p), not_red);
}

/* Simulacia hlavnej slucky: volanie Update kazdych step_ms az do casu "until", vypis zmien fazy */
static void run_until(uint32_t until, uint32_t step_ms, int print)
{
    TrafficPhase last = TrafficFsm_GetPhase();
    while (now < until)
    {
        now += step_ms;
        if (now > until) now = until;
        TrafficPhase p = TrafficFsm_Update(now);
        check_safety(p);
        if (p != last)
        {
            if (print) printf("  t=%5u ms  -> %s\n", (unsigned)now, phase_name(p));
            last = p;
        }
    }
}

/* ------------------------------------------------------------------------------ */

static void test_f1_demo_timing(void)
{
    /* ocakavane casy zmien podla tabulky Demo: 1 s startovacia cervena, potom cyklus 30 s */
    static const struct { uint32_t t; TrafficPhase p; } expected[] =
    {
        {1000, TRAFFIC_PHASE_A_GREEN}, {6000, TRAFFIC_PHASE_A_YELLOW}, {8000, TRAFFIC_PHASE_ALL_RED},
        {9000, TRAFFIC_PHASE_B_GREEN}, {14000, TRAFFIC_PHASE_B_YELLOW}, {16000, TRAFFIC_PHASE_ALL_RED},
        {17000, TRAFFIC_PHASE_C_GREEN}, {22000, TRAFFIC_PHASE_C_YELLOW}, {24000, TRAFFIC_PHASE_ALL_RED},
        {25000, TRAFFIC_PHASE_P_GREEN}, {30000, TRAFFIC_PHASE_ALL_RED},
    };
    const int n = (int)(sizeof(expected) / sizeof(expected[0]));

    printf("\nF1 - casovanie Demo (2 cykly, krok 1 ms)\n");
    now = 0;
    TrafficFsm_Init(now);
    expect_phase(TRAFFIC_PHASE_ALL_RED);

    for (int cycle = 0; cycle < 2; cycle++)
    {
        for (int i = 0; i < n; i++)
        {
            uint32_t t = expected[i].t + (uint32_t)cycle * 30000U;
            run_until(t - 1, 1, 0);
            CHECK(TrafficFsm_GetPhase() != expected[i].p, "faza %s zacala skor ako v case %u ms",
                  phase_name(expected[i].p), (unsigned)t);
            run_until(t, 1, 0);
            printf("  t=%5u ms  -> %s\n", (unsigned)now, phase_name(TrafficFsm_GetPhase()));
            expect_phase(expected[i].p);
        }
    }
    run_until(61000, 1, 0);
    expect_phase(TRAFFIC_PHASE_A_GREEN);          /* treti cyklus zacina fazou A */

    printf("\nF1b - oneskorene volanie Update (krok 7 ms): zlta a spolocna cervena dostanu cely interval\n");
    now = 0;
    TrafficFsm_Init(now);
    while (TrafficFsm_GetPhase() != TRAFFIC_PHASE_A_YELLOW)   /* hlavna slucka s periodou 7 ms */
    {
        now += 7;
        TrafficFsm_Update(now);
    }
    uint32_t yellow_start = now;
    printf("  zelena A skoncila v case %u ms (oneskorenie %u ms)\n", (unsigned)now, (unsigned)(now - 6000U));
    run_until(yellow_start + TRAFFIC_YELLOW_MS - 1, 1, 0);
    expect_phase(TRAFFIC_PHASE_A_YELLOW);
    run_until(yellow_start + TRAFFIC_YELLOW_MS, 1, 0);
    expect_phase(TRAFFIC_PHASE_ALL_RED);
    uint32_t red_start = now;
    run_until(red_start + TRAFFIC_ALL_RED_MS - 1, 1, 0);
    expect_phase(TRAFFIC_PHASE_ALL_RED);
    run_until(red_start + TRAFFIC_ALL_RED_MS, 1, 0);
    expect_phase(TRAFFIC_PHASE_B_GREEN);
    printf("  zlta trvala %u ms, spolocna cervena %u ms\n", (unsigned)TRAFFIC_YELLOW_MS, (unsigned)(now - red_start));
}

static void test_f2_conflicts(void)
{
    printf("\nF2 - kolizie v rezime Control\n");
    now = 0;
    TrafficFsm_Init(now);
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_A, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_ERR_MODE, "SET A GREEN");
    expect_result(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL, now), TRAFFIC_RESULT_OK, "MODE CONTROL");
    run_until(1000, 1, 1);
    expect_status(TRAFFIC_MODE_CONTROL, TRAFFIC_MODE_NONE, 1);

    /* zelena A, potom poziadavky na B a P */
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_A, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_OK, "SET A GREEN");
    expect_phase(TRAFFIC_PHASE_A_GREEN);
    run_until(3000, 1, 1);
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_B, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_ERR_CONFLICT, "SET B GREEN");
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_P, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_ERR_CONFLICT, "SET P GREEN");
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_A, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_OK, "SET A GREEN");
    expect_phase(TRAFFIC_PHASE_A_GREEN);

    /* ukoncenie A cez zltu - pocas zltej aj spolocnej cervenej sa zelena zamietne */
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_A, TRAFFIC_COLOR_RED, now), TRAFFIC_RESULT_OK, "SET A RED");
    expect_phase(TRAFFIC_PHASE_A_YELLOW);
    run_until(4000, 1, 1);
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_B, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_ERR_CONFLICT, "SET B GREEN");
    expect_status(TRAFFIC_MODE_CONTROL, TRAFFIC_MODE_NONE, 0);
    run_until(5500, 1, 1);
    expect_phase(TRAFFIC_PHASE_ALL_RED);
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_B, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_ERR_CONFLICT, "SET B GREEN");
    run_until(6000, 1, 1);
    expect_status(TRAFFIC_MODE_CONTROL, TRAFFIC_MODE_NONE, 1);

    /* po uplynuti spolocnej cervenej sa platny prikaz prijme - zelena P, potom C */
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_P, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_OK, "SET P GREEN");
    expect_phase(TRAFFIC_PHASE_P_GREEN);
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_C, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_ERR_CONFLICT, "SET C GREEN");
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_P, TRAFFIC_COLOR_YELLOW, now), TRAFFIC_RESULT_ERR_STATE, "SET P YELLOW");
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_B, TRAFFIC_COLOR_YELLOW, now), TRAFFIC_RESULT_ERR_STATE, "SET B YELLOW");
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_B, TRAFFIC_COLOR_RED, now), TRAFFIC_RESULT_OK, "SET B RED");
    expect_phase(TRAFFIC_PHASE_P_GREEN);
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_P, TRAFFIC_COLOR_RED, now), TRAFFIC_RESULT_OK, "SET P RED");
    expect_phase(TRAFFIC_PHASE_ALL_RED);
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_C, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_ERR_CONFLICT, "SET C GREEN");
    run_until(7000, 1, 1);
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_C, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_OK, "SET C GREEN");

    /* SET C YELLOW, pocas zltej SET C RED (zlta dobehne) a zelena trva do prikazu */
    run_until(60000, 10, 1);
    expect_phase(TRAFFIC_PHASE_C_GREEN);
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_C, TRAFFIC_COLOR_YELLOW, now), TRAFFIC_RESULT_OK, "SET C YELLOW");
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_C, TRAFFIC_COLOR_RED, now), TRAFFIC_RESULT_OK, "SET C RED");
    expect_phase(TRAFFIC_PHASE_C_YELLOW);
    run_until(63000, 1, 1);
    expect_status(TRAFFIC_MODE_CONTROL, TRAFFIC_MODE_NONE, 1);
}

static void test_f3_mode_switch(void)
{
    printf("\nF3 - prepnutie rezimu pocas cestnej zelenej a pocas zelenej pre chodcov\n");
    now = 0;
    TrafficFsm_Init(now);
    run_until(3000, 1, 1);                         /* Demo, zelena A */
    expect_phase(TRAFFIC_PHASE_A_GREEN);

    expect_result(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL, now), TRAFFIC_RESULT_OK, "MODE CONTROL");
    expect_phase(TRAFFIC_PHASE_A_YELLOW);          /* cestna zelena konci zltou */
    expect_status(TRAFFIC_MODE_DEMO, TRAFFIC_MODE_CONTROL, 0);
    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_P, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_ERR_BUSY, "SET P GREEN");
    expect_result(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO, now), TRAFFIC_RESULT_ERR_BUSY, "MODE DEMO");
    expect_result(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL, now), TRAFFIC_RESULT_OK, "MODE CONTROL");
    run_until(4999, 1, 1);
    expect_phase(TRAFFIC_PHASE_A_YELLOW);          /* zlta trva cele 2 s */
    run_until(5000, 1, 1);
    expect_phase(TRAFFIC_PHASE_ALL_RED);
    expect_status(TRAFFIC_MODE_DEMO, TRAFFIC_MODE_CONTROL, 0);
    run_until(6000, 1, 1);                         /* po 1 s spolocnej cervenej aktivny Control */
    expect_status(TRAFFIC_MODE_CONTROL, TRAFFIC_MODE_NONE, 1);
    run_until(20000, 10, 1);
    expect_phase(TRAFFIC_PHASE_ALL_RED);           /* Control caka na prikaz */

    expect_result(TrafficFsm_RequestColor(TRAFFIC_LIGHT_P, TRAFFIC_COLOR_GREEN, now), TRAFFIC_RESULT_OK, "SET P GREEN");
    run_until(22000, 1, 1);
    expect_result(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO, now), TRAFFIC_RESULT_OK, "MODE DEMO");
    expect_phase(TRAFFIC_PHASE_ALL_RED);           /* zelena pre chodcov -> spolocna cervena */
    expect_status(TRAFFIC_MODE_CONTROL, TRAFFIC_MODE_DEMO, 0);
    run_until(22999, 1, 1);
    expect_phase(TRAFFIC_PHASE_ALL_RED);
    run_until(23000, 1, 1);
    expect_phase(TRAFFIC_PHASE_A_GREEN);           /* Demo zacina fazou A */
    expect_status(TRAFFIC_MODE_DEMO, TRAFFIC_MODE_NONE, 0);
    expect_result(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO, now), TRAFFIC_RESULT_OK, "MODE DEMO");
    run_until(28000, 1, 1);
    expect_phase(TRAFFIC_PHASE_A_YELLOW);          /* cyklus pokracuje podla tabulky */

    printf("\nF3b - prepnutie po uplynuti spolocnej cervenej prebehne ihned\n");
    run_until(30500, 1, 1);                        /* Demo krok 3: spolocna cervena 30000-31000 */
    expect_result(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL, now), TRAFFIC_RESULT_OK, "MODE CONTROL");
    expect_status(TRAFFIC_MODE_DEMO, TRAFFIC_MODE_CONTROL, 0);
    run_until(31000, 1, 1);
    expect_status(TRAFFIC_MODE_CONTROL, TRAFFIC_MODE_NONE, 1);
    expect_result(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL, now), TRAFFIC_RESULT_OK, "MODE CONTROL");
    expect_result(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO, now), TRAFFIC_RESULT_OK, "MODE DEMO");
    expect_phase(TRAFFIC_PHASE_A_GREEN);           /* ochranny interval uz uplynul -> ihned Demo */
    expect_status(TRAFFIC_MODE_DEMO, TRAFFIC_MODE_NONE, 0);

    printf("\nF3c - pretecenie milisekundoveho casu (uint32_t)\n");
    now = 0xFFFFFC18U;                             /* 1 s pred pretecenim */
    TrafficFsm_Init(now);
    run_until(0xFFFFFFFFU, 1, 0);
    now = 0xFFFFFFFFU;
    expect_phase(TRAFFIC_PHASE_ALL_RED);
    now = 0U;                                      /* cas pretiekol, od Init uplynula 1 s */
    TrafficFsm_Update(now);
    printf("  t=%5u ms  -> %s\n", (unsigned)now, phase_name(TrafficFsm_GetPhase()));
    expect_phase(TRAFFIC_PHASE_A_GREEN);
}

/* Simulovany ovladac GPIO - callback registrovany v automate */
static TrafficPhase gpio_phase = TRAFFIC_PHASE_COUNT;
static unsigned gpio_calls = 0;

static void fake_gpio_apply(TrafficPhase p)
{
    gpio_phase = p;
    gpio_calls++;
}

static void test_callback(void)
{
    printf("\nCallback pre zmenu fazy (oddelenie automatu od GPIO)\n");

    /* bez registracie (NULL) automat funguje a nic nevola */
    TrafficFsm_RegisterPhaseCallback(0);
    now = 0;
    TrafficFsm_Init(now);
    run_until(2000, 1, 0);
    CHECK(gpio_calls == 0, "callback volany bez registracie");

    /* registracia pred Init: pociatocna spolocna cervena sa oznami hned */
    TrafficFsm_RegisterPhaseCallback(fake_gpio_apply);
    now = 0;
    TrafficFsm_Init(now);
    CHECK(gpio_calls == 1 && gpio_phase == TRAFFIC_PHASE_ALL_RED, "Init neoznamil spolocnu cervenu");

    /* pocas jedneho cyklu Demo: kazda zmena fazy = jedno volanie, GPIO ma vzdy aktualnu fazu */
    unsigned changes = 0;
    TrafficPhase last = TrafficFsm_GetPhase();
    while (now < 31000)
    {
        now++;
        TrafficPhase p = TrafficFsm_Update(now);
        if (p != last) { changes++; last = p; }
        CHECK(gpio_phase == p, "GPIO zobrazuje %s, automat je vo faze %s", phase_name(gpio_phase), phase_name(p));
    }
    printf("  zmien fazy: %u, volani callbacku: %u (vratane Init)\n", changes, gpio_calls);
    CHECK(gpio_calls == changes + 1U, "pocet volani callbacku %u, ocakavany %u", gpio_calls, changes + 1U);

    /* zmena fazy vyvolana prikazom sa oznami tiez */
    TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL, now);
    CHECK(gpio_phase == TrafficFsm_GetPhase(), "zmena fazy po MODE CONTROL nebola oznamena");

    TrafficFsm_RegisterPhaseCallback(0);
}

int main(void)
{
    test_callback();
    test_f1_demo_timing();
    test_f2_conflicts();
    test_f3_mode_switch();

    printf("\n%d kontrol, %d chyb -> %s\n", checks, failures, failures ? "NEUSPECH" : "VSETKY TESTY PRESLI");
    return failures ? 1 : 0;
}
