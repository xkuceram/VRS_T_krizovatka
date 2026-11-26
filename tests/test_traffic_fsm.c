/*
 * Testy stavoveho automatu T-krizovatky na PC so simulovanym casom.
 *
 *   cd tests && make
 *
 * Automat dostava cas a vstupy ako parametre, takze sa da testovat bez dosky.
 * Callback vystupov zaznamenava kazdu zmenu svetiel a pri kazdej zmene
 * kontroluje bezpecnost podla konfliktnej matice zapisanej v tomto teste
 * nezavisle od automatu:
 *   - dva konfliktne signaly nikdy nesvietia naraz (zelena ani zlta),
 *   - sipka A_S / C_S iba pri cervenej na A / C,
 *   - signal sa zapne az ked su on sam aj vsetky konfliktne signaly cervene aspon 1 s,
 *   - zlta trva cele 2 s, P nema zltu.
 *
 * Skusky podla README:
 *   F1 - obsluha vjazdu C (snimac, 5 s statia, sipka C_S)
 *   F2 - sipky, kolizie a chodci (Demo aj Control)
 *   F3 - prepnutie rezimu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "traffic_fsm.h"

/* ---------------------------------------------------------------------------
 * Signaly a konfliktna matica testu
 * ------------------------------------------------------------------------- */

enum { A, AS, BR, BD, C, CS, P, NSIG };
static const char *const sig_name[NSIG] = {"A", "A_S", "B_R", "B_D", "C", "C_S", "P"};

#define M(s) (1U << (s))

/* Odvodene z drah na obr. 1 v README (x = krizuju sa alebo zlievaju) */
static const unsigned conflict[NSIG] =
{
    [A]  = M(BD) | M(C) | M(CS) | M(P),
    [AS] = M(BD) | M(P),
    [BR] = M(C),
    [BD] = M(A) | M(AS) | M(C) | M(P),
    [C]  = M(A) | M(BR) | M(BD) | M(P),
    [CS] = M(A) | M(P),
    [P]  = M(A) | M(AS) | M(BD) | M(C) | M(CS)
};

/* Fazy Demo */
#define F1 (M(A) | M(BR))
#define F2 (M(C) | M(AS))
#define F3 (M(BR) | M(BD) | M(CS))
#define F4 (M(BR) | M(P))

/* ---------------------------------------------------------------------------
 * Pomocne funkcie
 * ------------------------------------------------------------------------- */

static int checks = 0;
static int failures = 0;
static int verbose = 0;                 /* 1 = vypis zmien svetiel */

static uint32_t now;                    /* simulovany cas */
static uint32_t t0;                     /* zaciatok testu (pre vypis) */

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        checks++;                                                           \
        if (!(cond)) {                                                      \
            failures++;                                                     \
            printf("  CHYBA (riadok %d, t=%lu ms): ", __LINE__,             \
                   (unsigned long)(now - t0));                              \
            printf(__VA_ARGS__);                                            \
            printf("\n");                                                   \
        }                                                                   \
    } while (0)

static bool car;                        /* stav snimaca CAR_C */
static bool press;                      /* jednorazove stlacenie tlacidla */

/* Stav svetiel z callbacku: 0 = cervena/zhasnuta, 1 = zelena, 2 = zlta */
static int state[NSIG];
static uint32_t since[NSIG];            /* kedy sa signal naposledy zmenil */
static TrafficSignals last;
static int changes;
static int greens_started;
static uint32_t br_red_ms;              /* ako dlho bola B_R nezelena (pre porovnanie plynulosti) */

static int to_state(TrafficColor c)
{
    return c == TRAFFIC_COLOR_GREEN ? 1 : (c == TRAFFIC_COLOR_YELLOW ? 2 : 0);
}

static unsigned green(void)
{
    unsigned m = 0;
    for (int s = 0; s < NSIG; s++)
    {
        if (state[s] == 1) m |= M(s);
    }
    return m;
}

static unsigned lit(void)
{
    unsigned m = 0;
    for (int s = 0; s < NSIG; s++)
    {
        if (state[s] != 0) m |= M(s);
    }
    return m;
}

static void print_state(void)
{
    static const char *const sym[3] = {".", "G", "Y"};
    printf("  t=%6.1f s ", (double)(now - t0) / 1000.0);
    for (int s = 0; s < NSIG; s++)
    {
        printf(" %s=%s", sig_name[s], sym[state[s]]);
    }
    printf("\n");
}

/*
 * Kontrola jednej zmeny svetiel voci stavu PRED zmenou.
 *   old, old_since - stav a cas poslednej zmeny kazdeho signalu pred zmenou
 *   ns             - novy stav, t_now - cas zmeny
 * Vrati pocet porusenych pravidiel. Pri report=true ich vypise.
 *
 * Signal, ktory sa zapina, musi byt pred zmenou cerveny aspon 1 s a kazdy
 * konfliktny signal tiez. Konfliktny signal, ktory svietil a zhasol v tej
 * istej milisekunde, je porusenie (0 ms ochrany).
 */
static int check_change(const int old[NSIG], const uint32_t old_since[NSIG],
                        const int ns[NSIG], uint32_t t_now, bool report)
{
    int bad = 0;

#define VIOLATION(...)                                                      \
    do {                                                                    \
        bad++;                                                              \
        if (report) {                                                       \
            printf("  CHYBA (t=%lu ms): ", (unsigned long)(t_now - t0));    \
            printf(__VA_ARGS__);                                            \
            printf("\n");                                                   \
        }                                                                   \
    } while (0)

    if (ns[P] == 2)
    {
        VIOLATION("P nema zltu");
    }

    for (int s = 0; s < NSIG; s++)
    {
        if (ns[s] == old[s])
        {
            continue;
        }
        if (ns[s] == 1)
        {
            /* zapnutie: sam aj konfliktne signaly cervene aspon 1 s pred touto zmenou */
            if (old[s] != 0 || t_now - old_since[s] < TRAFFIC_ALL_RED_MS)
            {
                VIOLATION("%s sa zapol bez 1 s cervenej (%lu ms)", sig_name[s],
                          (unsigned long)(t_now - old_since[s]));
            }
            for (int t = 0; t < NSIG; t++)
            {
                if ((conflict[s] & M(t)) != 0U &&
                    (old[t] != 0 || ns[t] != 0 || t_now - old_since[t] < TRAFFIC_ALL_RED_MS))
                {
                    VIOLATION("%s sa zapol %lu ms po konfliktnom %s (stav pred zmenou %d)",
                              sig_name[s], (unsigned long)(old[t] != 0 ? 0U : t_now - old_since[t]),
                              sig_name[t], old[t]);
                }
            }
        }
        else if (ns[s] == 2)
        {
            if (old[s] != 1)
            {
                VIOLATION("zlta %s iba po zelenej", sig_name[s]);
            }
        }
        else if (old[s] == 2)
        {
            if (t_now - old_since[s] < TRAFFIC_YELLOW_MS)
            {
                VIOLATION("zlta %s trvala iba %lu ms", sig_name[s], (unsigned long)(t_now - old_since[s]));
            }
        }
        else if (s != AS && s != CS && s != P)
        {
            VIOLATION("%s zhasol zo zelenej bez zltej", sig_name[s]);
        }
    }

    /* nikdy dva konfliktne signaly naraz, sipka iba pri cervenej na svojom semafore */
    for (int s = 0; s < NSIG; s++)
    {
        for (int t = s + 1; t < NSIG; t++)
        {
            if ((conflict[s] & M(t)) != 0U && ns[s] != 0 && ns[t] != 0)
            {
                VIOLATION("konflikt %s a %s", sig_name[s], sig_name[t]);
            }
        }
    }
    if (ns[AS] != 0 && ns[A] != 0)
    {
        VIOLATION("sipka A_S iba pri cervenej na A");
    }
    if (ns[CS] != 0 && ns[C] != 0)
    {
        VIOLATION("sipka C_S iba pri cervenej na C");
    }
#undef VIOLATION
    return bad;
}

/* Callback vystupov - kontrola bezpecnosti pri kazdej zmene */
static void on_output(const TrafficSignals *sig)
{
    int ns[NSIG];
    ns[A] = to_state(sig->a);
    ns[AS] = sig->a_s_on ? 1 : 0;
    ns[BR] = to_state(sig->b_r);
    ns[BD] = to_state(sig->b_d);
    ns[C] = to_state(sig->c);
    ns[CS] = sig->c_s_on ? 1 : 0;
    ns[P] = to_state(sig->p);

    changes++;
    const int bad = check_change(state, since, ns, now, true);
    CHECK(bad == 0, "%d porusenych pravidiel pri zmene svetiel", bad);

    for (int s = 0; s < NSIG; s++)
    {
        if (ns[s] != state[s])
        {
            if (ns[s] == 1)
            {
                greens_started++;
            }
            state[s] = ns[s];
            since[s] = now;
        }
    }

    if (verbose)
    {
        print_state();
    }
    last = *sig;
}

static void reset(uint32_t start)
{
    now = start;
    t0 = start;
    car = false;
    press = false;
    changes = 0;
    greens_started = 0;
    br_red_ms = 0;
    for (int s = 0; s < NSIG; s++)
    {
        state[s] = 0;
        since[s] = start;
    }
    TrafficFsm_RegisterOutputCallback(on_output);
    TrafficFsm_Init(now);
}

/* Posunie cas o ms v krokoch po 1 ms (hlavna slucka) */
static void run(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        now++;
        TrafficFsm_Update(now, car, press);
        press = false;
        if (state[BR] != 1)
        {
            br_red_ms++;
        }
    }
}

/* Posunie cas na absolutny cas testu (ms od reset) */
static void run_to(uint32_t t)
{
    if (t > now - t0)
    {
        run(t - (now - t0));
    }
}

static void press_button(void)
{
    press = true;
    run(1);
}

static TrafficFsmStatus status(void)
{
    TrafficFsmStatus st;
    TrafficFsm_GetStatus(&st);
    return st;
}

static TrafficResult set(TrafficTarget t, TrafficColor c)
{
    return TrafficFsm_RequestSignal(t, c);
}

/* Ocakavany stav: zelene signaly (maska) a zlte (maska) v case t */
static void expect_at(uint32_t t, unsigned greens, unsigned yellows, const char *what)
{
    run_to(t);
    unsigned y = lit() & ~green();
    CHECK(green() == greens && y == yellows, "%s: zelene 0x%02x (ocak. 0x%02x), zlte 0x%02x (ocak. 0x%02x)",
          what, green(), greens, y, yellows);
}

/* ---------------------------------------------------------------------------
 * Samotest kontroly ochranneho casu (check_change)
 * ------------------------------------------------------------------------- */

static int guard_case(int lit_before, uint32_t red_for_ms, int self_red_for_ms)
{
    /* B_D bol pred zmenou zlty (lit_before=2) alebo cerveny red_for_ms; v case 10 000 sa zapne A_S */
    int old[NSIG] = {0};
    uint32_t old_since[NSIG];
    int ns[NSIG] = {0};
    const uint32_t t_now = 10000U;

    for (int s = 0; s < NSIG; s++)
    {
        old_since[s] = 0U;
    }
    old[BD] = lit_before;
    old_since[BD] = t_now - red_for_ms;
    old_since[AS] = t_now - (uint32_t)self_red_for_ms;
    ns[AS] = 1;
    return check_change(old, old_since, ns, t_now, false);
}

static void test_guard_checker(void)
{
    printf("Samotest kontroly ochranneho casu voci stavu pred zmenou\n");
    t0 = now = 0;
    CHECK(guard_case(2, TRAFFIC_YELLOW_MS, 5000) > 0,
          "0 ms: B_D zlta zhasne a A_S sa zapne v tej istej ms -> porusenie");
    CHECK(guard_case(0, 0U, 5000) > 0, "0 ms: B_D cerveny prave teraz -> porusenie");
    CHECK(guard_case(0, 999U, 5000) > 0, "999 ms po cervenej B_D -> porusenie");
    CHECK(guard_case(0, 1000U, 5000) == 0, "1000 ms po cervenej B_D -> v poriadku");
    CHECK(guard_case(0, 1000U, 999) > 0, "A_S sam cerveny iba 999 ms -> porusenie");

    /* sipka hned po zltej na svojom semafore (ten isty pohyb) je povolena */
    int old[NSIG] = {0}, ns[NSIG] = {0};
    uint32_t old_since[NSIG] = {0};
    old[A] = 2;
    old_since[A] = 8000U;
    ns[AS] = 1;
    CHECK(check_change(old, old_since, ns, 10000U, false) == 0, "A zlta -> cervena a A_S v tej istej ms -> v poriadku");
    /* to iste so sipkou pri svietiacej A */
    old[A] = 1;
    ns[A] = 1;
    CHECK(check_change(old, old_since, ns, 10000U, false) > 0, "A_S pri zelenej A -> porusenie");
}

/* ---------------------------------------------------------------------------
 * F1 - Obsluha vjazdu C
 * ------------------------------------------------------------------------- */

static void test_f1(void)
{
    printf("F1a - volny vjazd C: F1 -> F3 -> F1, B_R ma zelenu nepretrzite\n");
    verbose = 1;
    reset(0);
    CHECK(changes == 1 && lit() == 0U, "po inicializacii vsetko cervene");
    expect_at(999, 0, 0, "prva sekunda spolocna cervena");
    expect_at(1000, F1, 0, "F1");
    expect_at(6000, M(BR), M(A), "A zlta, B_R pokracuje");
    expect_at(8000, M(BR), 0, "A cervena");
    expect_at(9000, F3, 0, "F3 vratane sipky C_S aj bez auta");
    expect_at(14000, M(BR), M(BD), "B_D zlta, C_S zhasne, B_R pokracuje");
    expect_at(17000, F1, 0, "opat F1, cyklus 16 s");
    run_to(33000);
    CHECK(br_red_ms == 999U, "B_R bola nezelena iba prvu sekundu (%lu ms)", (unsigned long)br_red_ms);
    verbose = 0;

    printf("F1b - auto stoji na C: F1 -> F2 (C oba smery + A_S) -> F3 -> F1\n");
    verbose = 1;
    reset(0);
    car = true;                         /* snimac sa precita v prvom kroku (t = 1 ms) */
    expect_at(1000, F1, 0, "F1");
    run_to(5000);
    CHECK(!status().c_req, "po 4,999 s statia este C_REQ=0");
    run(1);
    CHECK(status().c_req, "po 5 s statia C_REQ=1");
    expect_at(6000, 0, M(A) | M(BR), "A aj B_R zlta");
    expect_at(8000, M(AS), 0, "A_S hned po zltej A (ten isty pohyb)");
    expect_at(9000, F2, 0, "F2: C a A_S");
    CHECK(!status().c_req, "C_REQ=0 po zapnuti C");
    expect_at(14000, 0, M(C), "C zlta, A_S zhasne");
    expect_at(16000, M(CS), 0, "sipka C_S hned po cervenej na C");
    expect_at(17000, F3, 0, "F3");
    expect_at(22000, M(BR), M(BD), "B_D zlta, B_R pokracuje");
    expect_at(25000, F1, 0, "opat F1, cyklus 24 s");
    verbose = 0;

    printf("F1c - auto snimacom len prejde (2 s): faza C sa nezaradi\n");
    reset(0);
    run_to(2000);
    car = true;
    run(2000);
    car = false;
    expect_at(9000, F3, 0, "po F1 rovno F3");
    CHECK(!status().c_req, "C_REQ=0");

    printf("F1d - hranica 5 s: 4,999 s nestaci, 5 s ano\n");
    reset(0);
    car = true;                         /* snimac sa precita v prvom kroku (t = 1 ms) */
    run_to(5000);
    CHECK(!status().c_req, "po 4,999 s este C_REQ=0");
    run(1);
    CHECK(status().c_req, "po 5 s C_REQ=1");
    car = false;
    expect_at(9000, F2, 0, "poziadavka sa drzi aj ked auto odide pri cervenej");

    printf("F1e - auto pocka 5 s a odboci doprava po sipke C_S: poziadavka sa zrusi\n");
    reset(0);
    run_to(17500);                      /* F1 druheho cyklu */
    car = true;
    run_to(22600);
    CHECK(status().c_req, "C_REQ=1 (zapisane po skonceni F1)");
    expect_at(25000, F3, 0, "F3 (C_REQ prisiel neskoro na F2)");
    run_to(26000);
    car = false;                        /* odbocil doprava po sipke: obsadeny -> volny pri C_S */
    run(1);
    CHECK(!status().c_req, "C_REQ=0 po odchode pocas sipky");
    expect_at(41000, F3, 0, "dalsi cyklus bez F2");

    printf("F1e2 - auto odide este pred sipkou: pri zapnuti C_S je snimac volny, poziadavka ostava\n");
    reset(0);
    run_to(17500);
    car = true;
    run_to(22600);
    CHECK(status().c_req, "C_REQ=1");
    run_to(23000);
    car = false;                        /* odislo pri cervenej, pred zapnutim C_S */
    expect_at(24999, M(BR), 0, "C_S este nesvieti");
    expect_at(25000, F3, 0, "C_S zapne pri volnom snimaci");
    run_to(30000);
    CHECK(status().c_req, "C_REQ=1 aj po celej sipke (snimac nezmenil stav)");
    expect_at(41000, F2, 0, "F2 po najblizsej F1");

    printf("F1e3 - auto pride az pocas sipky a hned odide: bez poziadavky nic nerusi\n");
    reset(0);
    run_to(9500);
    car = true;
    run(500);
    car = false;
    CHECK(!status().c_req, "C_REQ=0");

    printf("F1f - auto ostane stat: F2 pride po najblizsej F1\n");
    reset(0);
    run_to(17500);
    car = true;
    expect_at(25000, F3, 0, "F3");
    expect_at(33000, F1, 0, "F1");
    expect_at(41000, F2, 0, "F2 po F1");
}

/* ---------------------------------------------------------------------------
 * F2 - Sipky, kolizie a chodci
 * ------------------------------------------------------------------------- */

static void test_f2(void)
{
    printf("F2a - chodec pocas F1: F1 -> F4 (P + B_R) -> F3 -> F1, B_R nezastavi\n");
    verbose = 1;
    reset(0);
    run_to(2000);
    press_button();
    CHECK(status().p_req, "P_REQ=1");
    expect_at(6000, M(BR), M(A), "A zlta");
    expect_at(9000, F4, 0, "F4: chodci a B_R");
    CHECK(!status().p_req, "P_REQ=0 po zapnuti P");
    press_button();
    CHECK(!status().p_req, "stlacenie pocas zelenej P patri do prebiehajucej fazy");
    expect_at(14000, M(BR), 0, "P rovno na cervenu, B_R pokracuje");
    expect_at(15000, F3, 0, "F3 po 1 s ochrany za chodcami");
    expect_at(25000, F1, 0, "F1");
    CHECK(br_red_ms == 999U, "B_R ani raz nezastavila (%lu ms nezelena)", (unsigned long)br_red_ms);
    verbose = 0;

    printf("F2b - chodec pocas F3: F3 -> F4 -> F1\n");
    reset(0);
    run_to(10000);
    press_button();
    expect_at(14000, M(BR), M(BD), "B_D zlta");
    expect_at(17000, F4, 0, "F4");
    expect_at(22000, M(BR), 0, "P cervena");
    expect_at(23000, F1, 0, "F1 po chodcoch");

    printf("F2c - chodec aj auto na C: F1 -> F4 -> F2 -> F3\n");
    verbose = 1;
    reset(0);
    car = true;
    run_to(2000);
    press_button();
    expect_at(9000, F4, 0, "F4");
    expect_at(14000, 0, M(BR), "B_R zlta (konflikt s C)");
    expect_at(15000, M(AS), M(BR), "A_S 1 s po chodcoch");
    expect_at(17000, F2, 0, "F2 1 s po cervenej B_R");
    expect_at(25000, F3, 0, "F3");
    verbose = 0;

    printf("F2d - Control: zelena sa prida, ak je podla matice volna\n");
    reset(0);
    CHECK(set(TRAFFIC_TARGET_A, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_MODE, "SET v Demo -> ERR MODE");
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL) == TRAFFIC_RESULT_OK, "MODE CONTROL");
    run(TRAFFIC_ALL_RED_MS);
    CHECK(status().mode == TRAFFIC_MODE_CONTROL && status().ready, "Control, READY=1");
    CHECK(set(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET B_D GREEN");
    CHECK(!status().ready, "READY=0 ked nieco svieti");
    CHECK(set(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "opakovany SET potvrdi");
    CHECK(set(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT, "A_S pri B_D -> ERR CONFLICT");
    CHECK(set(TRAFFIC_TARGET_P, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT, "P pri B_D -> ERR CONFLICT");
    CHECK(set(TRAFFIC_TARGET_C_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "C_S pri B_D -> OK");
    CHECK(set(TRAFFIC_TARGET_B, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET B doplni B_R");
    CHECK(green() == F3, "rucne zostavena F3");
    CHECK(set(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_YELLOW) == TRAFFIC_RESULT_ERR_STATE, "SET A_S YELLOW -> ERR STATE");
    CHECK(set(TRAFFIC_TARGET_A, TRAFFIC_COLOR_YELLOW) == TRAFFIC_RESULT_ERR_STATE, "SET A YELLOW pri cervenej A -> ERR STATE");
    CHECK(set(TRAFFIC_TARGET_A, TRAFFIC_COLOR_RED) == TRAFFIC_RESULT_OK, "SET A RED pri cervenej A potvrdi");
    run(10000);
    CHECK(green() == F3, "v Control zelena trva do prikazu");
    CHECK(set(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_RED) == TRAFFIC_RESULT_OK, "SET B_D RED");
    CHECK(state[BD] == 2 && state[BR] == 1 && state[CS] == 1, "konci iba B_D (zlta), ostatne svietia");
    CHECK(set(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT, "A_S pocas zltej B_D -> ERR CONFLICT");
    run(TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS - 1U);
    CHECK(set(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT, "999 ms po cervenej B_D este ERR CONFLICT");
    run(1);
    CHECK(set(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "A_S po 1 s -> OK");
    CHECK(green() == (M(AS) | M(BR) | M(CS)), "F5: A_S, B_R a C_S naraz");

    printf("F2e - spolocna skuska bod 3: A_S, ERR CONFLICT pre B_D, potom B_D\n");
    set(TRAFFIC_TARGET_B_R, TRAFFIC_COLOR_RED);
    set(TRAFFIC_TARGET_C_S, TRAFFIC_COLOR_RED);
    run(TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    CHECK(green() == M(AS), "svieti iba A_S");
    CHECK(set(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT, "SET B_D GREEN pri A_S -> ERR CONFLICT");
    CHECK(set(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_RED) == TRAFFIC_RESULT_OK, "SET A_S RED");
    CHECK(lit() == 0U, "sipka zhasne hned");
    run(TRAFFIC_ALL_RED_MS);
    CHECK(set(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET B_D GREEN po 1 s");

    printf("F2f - C a sipka C_S: sipka iba pri cervenej C, hned po zltej\n");
    set(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_RED);
    run(TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    CHECK(set(TRAFFIC_TARGET_C, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET C GREEN");
    CHECK(set(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "A_S k C -> OK (F2)");
    CHECK(set(TRAFFIC_TARGET_C_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT, "C_S pri zelenej C -> ERR CONFLICT");
    CHECK(set(TRAFFIC_TARGET_B_R, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT, "B_R pri C -> ERR CONFLICT");
    CHECK(set(TRAFFIC_TARGET_C, TRAFFIC_COLOR_YELLOW) == TRAFFIC_RESULT_OK, "SET C YELLOW");
    CHECK(set(TRAFFIC_TARGET_C_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT, "C_S pocas zltej C -> ERR CONFLICT");
    run(TRAFFIC_YELLOW_MS);
    CHECK(set(TRAFFIC_TARGET_C_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "C_S hned po cervenej C");

    printf("F2g - chodci a auto v Control: P_REQ a C_REQ vybavia prikazy\n");
    set(TRAFFIC_TARGET_C_S, TRAFFIC_COLOR_RED);
    set(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_RED);
    run(TRAFFIC_ALL_RED_MS);
    press_button();
    car = true;
    run(TRAFFIC_CAR_WAIT_MS + 1U);
    CHECK(status().p_req && status().c_req && status().car_c, "P_REQ=1 C_REQ=1 CAR_C=1");
    CHECK(lit() == 0U, "v Control sa nic samo nezapne");
    CHECK(set(TRAFFIC_TARGET_P, TRAFFIC_COLOR_YELLOW) == TRAFFIC_RESULT_ERR_STATE, "SET P YELLOW -> ERR STATE");
    CHECK(set(TRAFFIC_TARGET_P, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET P GREEN");
    CHECK(set(TRAFFIC_TARGET_B_R, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "B_R k chodcom -> OK (F4)");
    CHECK(set(TRAFFIC_TARGET_A, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT, "A pri chodcoch -> ERR CONFLICT");
    CHECK(!status().p_req, "P_REQ=0");
    set(TRAFFIC_TARGET_P, TRAFFIC_COLOR_RED);
    set(TRAFFIC_TARGET_B_R, TRAFFIC_COLOR_RED);
    CHECK(state[P] == 0, "P prejde rovno na cervenu");
    run(TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    CHECK(set(TRAFFIC_TARGET_C, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK && !status().c_req, "SET C GREEN, C_REQ=0");

    printf("F2h - neplatne parametre\n");
    CHECK(set(TRAFFIC_TARGET_COUNT, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_PARAM, "neplatny ciel");
    CHECK(set(TRAFFIC_TARGET_A, (TrafficColor)7) == TRAFFIC_RESULT_ERR_PARAM, "neplatna farba");
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_NONE) == TRAFFIC_RESULT_ERR_PARAM, "neplatny rezim");
}

/* ---------------------------------------------------------------------------
 * F3 - Prepnutie rezimu
 * ------------------------------------------------------------------------- */

static void test_f3(void)
{
    printf("F3a - pocas F3 stlacit tlacidlo a prepnut do Control\n");
    verbose = 1;
    reset(0);
    run_to(10000);
    CHECK(green() == F3, "F3");
    press_button();
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL) == TRAFFIC_RESULT_OK, "MODE CONTROL");
    CHECK(state[CS] == 0 && state[BR] == 2 && state[BD] == 2, "C_S zhasne hned, oba B na zltu");
    TrafficFsmStatus st = status();
    CHECK(st.mode == TRAFFIC_MODE_DEMO && st.next_mode == TRAFFIC_MODE_CONTROL && !st.ready,
          "pocas prepinania MODE=DEMO NEXT=CONTROL READY=0");
    CHECK(set(TRAFFIC_TARGET_P, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_BUSY, "SET pocas prepinania -> ERR BUSY");
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO) == TRAFFIC_RESULT_ERR_BUSY, "navrat do opustaneho rezimu -> ERR BUSY");
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL) == TRAFFIC_RESULT_OK, "opakovany ten isty ciel -> OK");
    run(TRAFFIC_YELLOW_MS);
    CHECK(lit() == 0U && status().next_mode == TRAFFIC_MODE_CONTROL, "spolocna cervena, prepinanie trva");
    run(TRAFFIC_ALL_RED_MS);
    st = status();
    CHECK(st.mode == TRAFFIC_MODE_CONTROL && st.next_mode == TRAFFIC_MODE_NONE && st.ready && st.p_req,
          "MODE=CONTROL NEXT=- READY=1 P_REQ=1");
    run(10000);
    CHECK(lit() == 0U, "Control caka pri spolocnej cervenej");
    CHECK(set(TRAFFIC_TARGET_P, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET P GREEN");
    run(2000);
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO) == TRAFFIC_RESULT_OK, "MODE DEMO pocas zelenej P");
    CHECK(lit() == 0U, "P rovno na cervenu");
    run(TRAFFIC_ALL_RED_MS - 1U);
    CHECK(lit() == 0U && status().next_mode == TRAFFIC_MODE_DEMO, "ochranna sekunda");
    run(1);
    CHECK(green() == F1 && status().mode == TRAFFIC_MODE_DEMO, "Demo zacne fazou F1");
    verbose = 0;

    printf("F3b - prepnutie pri uz uplynutej ochrannej sekunde je okamzite\n");
    reset(0);
    TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL);
    run(5000);
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO) == TRAFFIC_RESULT_OK, "MODE DEMO");
    CHECK(status().mode == TRAFFIC_MODE_DEMO && green() == F1, "prepne hned a zacne F1");
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO) == TRAFFIC_RESULT_OK, "MODE DEMO v Demo potvrdi");

    printf("F3c - zmena rezimu pocas cestnej zelenej v Control (cez zltu)\n");
    TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL);
    run(TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    set(TRAFFIC_TARGET_C, TRAFFIC_COLOR_GREEN);
    set(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_GREEN);
    TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO);
    CHECK(state[C] == 2 && state[AS] == 0, "C dobehne cez zltu, sipka zhasne hned");
    run(TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    CHECK(green() == F1, "Demo od F1");

    printf("F3d - cakajuce poziadavky sa prenesu do Demo\n");
    reset(0);
    TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL);
    car = true;
    run(TRAFFIC_CAR_WAIT_MS + 1U);
    press_button();
    TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO);
    CHECK(green() == F1 && status().p_req && status().c_req, "F1, P_REQ=1, C_REQ=1");
    run(TRAFFIC_GREEN_MS + TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    CHECK(green() == F4, "chodci po F1");
    run(TRAFFIC_GREEN_MS + TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    CHECK(green() == F2, "potom vjazd C");
}

/* ---------------------------------------------------------------------------
 * Dalsie kontroly
 * ------------------------------------------------------------------------- */

static void test_overflow(void)
{
    printf("Pretecenie milisekundoveho casu (uint32_t)\n");
    reset(0xFFFFFFFFU - 3000U);
    run(1000);
    CHECK(green() == F1, "F1 pred pretecenim");
    run(8000);
    CHECK(now < 10000U && green() == F3, "F3 po preteceni");
}

static void test_callback_null(void)
{
    printf("Automat bez callbacku (NULL)\n");
    TrafficFsm_RegisterOutputCallback(NULL);
    TrafficFsm_Init(0);
    for (uint32_t t = 1; t < 20000U; t++)
    {
        TrafficFsm_Update(t, false, false);
    }
    TrafficFsmStatus st;
    TrafficFsm_GetStatus(&st);
    TrafficFsm_GetStatus(NULL);
    now = t0 = 0;
    CHECK(st.mode == TRAFFIC_MODE_DEMO, "bezi aj bez callbacku");
}

/* Dlha nahodna simulacia: vstupy aj prikazy nahodne, bezpecnost sa kontroluje v on_output */
static void test_random(void)
{
    printf("Nahodna simulacia 2 hodiny (vstupy aj prikazy), kontrola matice pri kazdej zmene\n");
    srand(12345U);
    reset(0);
    const int start_failures = failures;
    for (uint32_t i = 0; i < 2U * 3600U * 1000U; i++)
    {
        if (rand() % 8000 == 0) car = !car;
        if (rand() % 15000 == 0) press = true;
        if (rand() % 1500 == 0)
        {
            if (rand() % 12 == 0)
            {
                TrafficFsm_RequestMode((TrafficMode)(rand() % 2));
            }
            else
            {
                TrafficFsm_RequestSignal((TrafficTarget)(rand() % TRAFFIC_TARGET_COUNT),
                                         (TrafficColor)(rand() % 3));
            }
        }
        run(1);
        if (failures - start_failures > 20) break;
    }
    printf("  zmien svetiel: %d, zapnutych signalov: %d\n", changes, greens_started);
    CHECK(greens_started > 1000, "simulacia presla mnoho zmien");
}

int main(void)
{
    test_guard_checker();
    test_f1();
    test_f2();
    test_f3();
    test_overflow();
    test_callback_null();
    test_random();

    printf("\n%d kontrol, %d chyb -> %s\n", checks, failures,
           failures == 0 ? "VSETKY TESTY PRESLI" : "NIEKTORE TESTY ZLYHALI");
    return failures == 0 ? 0 : 1;
}
