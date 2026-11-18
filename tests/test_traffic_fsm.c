/*
 * Testy stavoveho automatu T-krizovatky na PC so simulovanym casom.
 *
 *   cd tests && make
 *
 * Automat dostava cas a vstupy ako parametre, takze sa da testovat bez dosky.
 * Callback vystupov zaznamenava kazdu zmenu svetiel s casom. Pri kazdej zmene
 * sa kontroluju bezpecnostne pravidla (povolene kombinacie, zlta 2 s,
 * spolocna cervena aspon 1 s pred kazdou novou zelenou).
 *
 * Skusky podla README:
 *   F1 - obsluha vjazdu C (volny / obsadeny vjazd, uvolnenie pocas C dolava)
 *   F2 - sipky, kolizie a chodci (Demo aj Control)
 *   F3 - prepnutie rezimu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "traffic_fsm.h"

/* ---------------------------------------------------------------------------
 * Pomocne funkcie
 * ------------------------------------------------------------------------- */

static int checks = 0;
static int failures = 0;
static int verbose = 0;                 /* 1 = vypis zmien svetiel */

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

static uint32_t now;                    /* simulovany cas */
static uint32_t t0;                     /* zaciatok testu (pre vypis) */
static bool car;                        /* stav snimaca CAR_C */
static bool press;                      /* jednorazove stlacenie tlacidla */

/* Posledny stav z callbacku a casy pre kontrolu intervalov */
static TrafficSignals last;
static int changes;
static uint32_t all_red_since;          /* kedy zacala spolocna cervena */
static bool in_all_red;
static uint32_t yellow_since;
static bool in_yellow;
static int greens_started;

static const char *col(TrafficColor c)
{
    return c == TRAFFIC_COLOR_GREEN ? "G" : (c == TRAFFIC_COLOR_YELLOW ? "Y" : "r");
}

static void print_signals(const TrafficSignals *s)
{
    printf("A=%s A_S=%s B_R=%s B_D=%s C=%s C_S=%s P=%s",
           col(s->a), s->a_s_on ? "ON " : "off", col(s->b_r), col(s->b_d),
           col(s->c), s->c_s_on ? "ON " : "off", col(s->p));
}

static bool any_green(const TrafficSignals *s)
{
    return s->a == TRAFFIC_COLOR_GREEN || s->b_r == TRAFFIC_COLOR_GREEN ||
           s->b_d == TRAFFIC_COLOR_GREEN || s->c == TRAFFIC_COLOR_GREEN ||
           s->p == TRAFFIC_COLOR_GREEN || s->a_s_on || s->c_s_on;
}

static bool any_yellow(const TrafficSignals *s)
{
    return s->a == TRAFFIC_COLOR_YELLOW || s->b_r == TRAFFIC_COLOR_YELLOW ||
           s->b_d == TRAFFIC_COLOR_YELLOW || s->c == TRAFFIC_COLOR_YELLOW;
}

static bool all_red(const TrafficSignals *s)
{
    return !any_green(s) && !any_yellow(s) && s->p == TRAFFIC_COLOR_RED;
}

/* Maska zelenych signalov: A, A_S, B_R, B_D, C, C_S, P */
static unsigned green_mask(const TrafficSignals *s)
{
    return (s->a == TRAFFIC_COLOR_GREEN ? 1U : 0U) | (s->a_s_on ? 2U : 0U) |
           (s->b_r == TRAFFIC_COLOR_GREEN ? 4U : 0U) | (s->b_d == TRAFFIC_COLOR_GREEN ? 8U : 0U) |
           (s->c == TRAFFIC_COLOR_GREEN ? 16U : 0U) | (s->c_s_on ? 32U : 0U) |
           (s->p == TRAFFIC_COLOR_GREEN ? 64U : 0U);
}

#define M_A    1U
#define M_AS   2U
#define M_BR   4U
#define M_BD   8U
#define M_C    16U
#define M_CS   32U
#define M_P    64U

/* Povolene mnoziny zelenych: fazy Demo a jednotlive volby v Control */
static bool allowed_green_set(unsigned m)
{
    static const unsigned allowed[] =
    {
        M_A | M_BR, M_C | M_AS, M_BR | M_BD | M_CS, M_BR | M_BD, M_P,      /* Demo */
        M_A, M_AS, M_BR, M_BD, M_C, M_CS, M_BR | M_BD                       /* Control */
    };
    for (size_t i = 0; i < sizeof allowed / sizeof allowed[0]; i++)
    {
        if (m == allowed[i])
        {
            return true;
        }
    }
    return false;
}

/* Callback vystupov - kontrola pravidiel pri kazdej zmene */
static void on_output(const TrafficSignals *s)
{
    const unsigned m = green_mask(s);
    const bool was_all_red = in_all_red;

    changes++;
    if (verbose)
    {
        printf("  t=%6lu ms  ", (unsigned long)(now - t0));
        print_signals(s);
        printf("\n");
    }

    /* pravidla signalizacie */
    CHECK(s->p != TRAFFIC_COLOR_YELLOW, "P nema zltu");
    CHECK(!s->a_s_on || s->a == TRAFFIC_COLOR_RED, "sipka A_S iba pri cervenej na A");
    CHECK(!s->c_s_on || s->c == TRAFFIC_COLOR_RED, "sipka C_S iba pri cervenej na C");
    CHECK(!(s->a_s_on && s->b_d == TRAFFIC_COLOR_GREEN), "A_S a B_D vedu do toho isteho pruhu");
    CHECK(m == 0U || allowed_green_set(m), "nepovolena kombinacia zelenych 0x%02x", m);
    if (s->p == TRAFFIC_COLOR_GREEN)
    {
        CHECK(m == M_P && !any_yellow(s), "pri zelenej pre chodcov musi byt vsetko ostatne cervene");
    }
    CHECK(!(any_green(s) && any_yellow(s)), "zelena a zlta naraz");

    /* zlta trva cele 2 s */
    if (in_yellow && !any_yellow(s))
    {
        CHECK((uint32_t)(now - yellow_since) >= TRAFFIC_YELLOW_MS,
              "zlta trvala iba %lu ms", (unsigned long)(now - yellow_since));
        CHECK(all_red(s), "po zltej musi nasledovat spolocna cervena");
    }
    if (!in_yellow && any_yellow(s))
    {
        yellow_since = now;
        CHECK(!was_all_red, "zlta moze nasledovat iba po zelenej");
    }
    in_yellow = any_yellow(s);

    /* nova zelena iba po aspon 1 s spolocnej cervenej */
    if (any_green(s))
    {
        CHECK(was_all_red, "zelena bez predchadzajucej spolocnej cervenej");
        if (was_all_red)
        {
            CHECK((uint32_t)(now - all_red_since) >= TRAFFIC_ALL_RED_MS,
                  "spolocna cervena trvala iba %lu ms", (unsigned long)(now - all_red_since));
        }
        greens_started++;
    }
    if (all_red(s) && !in_all_red)
    {
        all_red_since = now;
    }
    in_all_red = all_red(s);

    last = *s;
}

static void reset(uint32_t start)
{
    now = start;
    t0 = start;
    car = false;
    press = false;
    changes = 0;
    in_all_red = false;
    in_yellow = false;
    greens_started = 0;
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
    }
}

/* Jedno stlacenie tlacidla v najblizsom kroku */
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

static void expect_green(unsigned expected, const char *name)
{
    CHECK(green_mask(&last) == expected, "%s: ocakavane 0x%02x, je 0x%02x", name, expected, green_mask(&last));
}

/* Overi celu cestnu fazu Demo: zelena 5 s, zlta 2 s (ak ma bezne semafory), cervena 1 s */
static void demo_phase(unsigned expected, bool has_yellow, const char *name)
{
    expect_green(expected, name);
    run(TRAFFIC_GREEN_MS - 1U);
    expect_green(expected, name);
    run(1);
    if (has_yellow)
    {
        CHECK(any_yellow(&last) && !last.a_s_on && !last.c_s_on, "%s: po 5 s zlta a zhasnute sipky", name);
        run(TRAFFIC_YELLOW_MS);
    }
    CHECK(all_red(&last), "%s: spolocna cervena", name);
    run(TRAFFIC_ALL_RED_MS);
}

/* ---------------------------------------------------------------------------
 * F1 - Obsluha vjazdu C
 * ------------------------------------------------------------------------- */

static void test_f1(void)
{
    printf("F1a - volny vjazd C: A -> B -> A\n");
    verbose = 1;
    reset(0);
    CHECK(all_red(&last) && changes == 1, "po inicializacii pat cervenych a zhasnute sipky");
    run(TRAFFIC_ALL_RED_MS - 1U);
    CHECK(all_red(&last), "prva sekunda spolocna cervena");
    run(1);
    demo_phase(M_A | M_BR, true, "A");
    CHECK(last.b_r == TRAFFIC_COLOR_GREEN && last.b_d == TRAFFIC_COLOR_GREEN && last.c_s_on == false,
          "B pri volnom vjazde C: B_R a B_D zelene, C_S zhasnuta");
    demo_phase(M_BR | M_BD, true, "B volny");
    expect_green(M_A | M_BR, "opat A");
    CHECK(now - t0 == 1000U + 2U * 8000U, "cyklus A -> B trva 16 s");
    verbose = 0;

    printf("F1b - obsadeny vjazd C: A -> C dolava -> B s C_S -> A\n");
    verbose = 1;
    reset(0);
    car = true;
    run(TRAFFIC_ALL_RED_MS);
    demo_phase(M_A | M_BR, true, "A");
    CHECK(last.c == TRAFFIC_COLOR_GREEN && last.a_s_on && last.a == TRAFFIC_COLOR_RED,
          "C dolava: zelena C, sipka A_S pri cervenej na A");
    demo_phase(M_C | M_AS, true, "C dolava");
    CHECK(last.c_s_on && last.c == TRAFFIC_COLOR_RED, "B s C_S: sipka C_S pri cervenej na C");
    demo_phase(M_BR | M_BD | M_CS, true, "B s C_S");
    expect_green(M_A | M_BR, "opat A");
    verbose = 0;

    printf("F1c - auto odide pocas C dolava, faza s C_S aj tak prebehne\n");
    reset(0);
    car = true;
    run(TRAFFIC_ALL_RED_MS);
    demo_phase(M_A | M_BR, true, "A");
    run(1000);
    car = false;                            /* auto odislo */
    run(TRAFFIC_GREEN_MS - 1000U + TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    demo_phase(M_BR | M_BD | M_CS, true, "B s C_S");
    demo_phase(M_A | M_BR, true, "A");
    expect_green(M_BR | M_BD, "dalsi cyklus pri volnom vjazde: B bez C_S");

    printf("F1d - auto pride az pocas B, vyhodnoti sa po dalsej faze A\n");
    reset(0);
    run(TRAFFIC_ALL_RED_MS);
    demo_phase(M_A | M_BR, true, "A");
    car = true;
    demo_phase(M_BR | M_BD, true, "B volny");
    demo_phase(M_A | M_BR, true, "A");
    expect_green(M_C | M_AS, "C dolava po faze A");
}

/* ---------------------------------------------------------------------------
 * F2 - Sipky, kolizie a chodci
 * ------------------------------------------------------------------------- */

static void test_f2(void)
{
    printf("F2a - tlacidlo pocas zelenej A: P az po zltej a spolocnej cervenej\n");
    verbose = 1;
    reset(0);
    run(TRAFFIC_ALL_RED_MS + 1000U);
    expect_green(M_A | M_BR, "A");
    press_button();
    CHECK(status().p_req, "P_REQ=1 po stlaceni");
    run(TRAFFIC_GREEN_MS - 1001U);
    CHECK(any_yellow(&last), "A konci zltou");
    run(TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    expect_green(M_P, "chodci");
    CHECK(!status().p_req, "P_REQ=0 po zapnuti zelenej pre P");
    press_button();
    CHECK(!status().p_req, "stlacenie pocas zelenej P patri do prebiehajucej fazy");
    run(TRAFFIC_GREEN_MS - 1U);
    CHECK(all_red(&last), "po P rovno spolocna cervena (bez zltej)");
    run(TRAFFIC_ALL_RED_MS);
    expect_green(M_BR | M_BD, "po chodcoch naplanovana faza B");
    verbose = 0;

    printf("F2b - pri obsadenom C ostane po chodcoch naplanovane C dolava\n");
    reset(0);
    car = true;
    run(TRAFFIC_ALL_RED_MS + 100U);
    press_button();
    run(TRAFFIC_GREEN_MS + TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    expect_green(M_P, "chodci po A");
    car = false;                            /* rozhodnutie uz padlo pred chodcami */
    run(TRAFFIC_GREEN_MS + TRAFFIC_ALL_RED_MS);
    expect_green(M_C | M_AS, "C dolava po chodcoch");

    printf("F2c - kolizie v Control\n");
    reset(0);
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_A, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_MODE,
          "SET v Demo -> ERR MODE");
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL) == TRAFFIC_RESULT_OK, "MODE CONTROL");
    run(TRAFFIC_ALL_RED_MS);
    CHECK(status().mode == TRAFFIC_MODE_CONTROL && status().ready, "Control, READY=1");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET B_D GREEN");
    expect_green(M_BD, "B_D");
    CHECK(!status().ready, "READY=0 pri aktivnej faze");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "opakovany SET potvrdi");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT,
          "SET A_S GREEN pri zelenej B_D -> ERR CONFLICT");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT,
          "SET B GREEN pri zelenej iba B_D -> ERR CONFLICT");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_YELLOW) == TRAFFIC_RESULT_ERR_STATE,
          "SET A_S YELLOW -> ERR STATE");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_A, TRAFFIC_COLOR_YELLOW) == TRAFFIC_RESULT_ERR_STATE,
          "SET A YELLOW pri cervenej A -> ERR STATE");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_A, TRAFFIC_COLOR_RED) == TRAFFIC_RESULT_OK,
          "SET A RED pri cervenej A potvrdi stav");
    expect_green(M_BD, "B_D stale zelena");
    run(10000);
    expect_green(M_BD, "v Control zelena trva do prikazu");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_YELLOW) == TRAFFIC_RESULT_OK, "SET B_D YELLOW");
    CHECK(last.b_d == TRAFFIC_COLOR_YELLOW, "B_D zlta");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT,
          "nova zelena pocas zltej -> ERR CONFLICT");
    run(TRAFFIC_YELLOW_MS);
    CHECK(all_red(&last), "po zltej spolocna cervena");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT,
          "nova zelena pocas ochrannej cervenej -> ERR CONFLICT");
    run(TRAFFIC_ALL_RED_MS);
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET A_S GREEN po intervale");
    CHECK(last.a_s_on && last.a == TRAFFIC_COLOR_RED, "sipka A_S pri cervenej na A");

    printf("F2d - spolocna skuska bod 3: A_S, ERR CONFLICT pre B_D, potom B_D\n");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT,
          "SET B_D GREEN pri A_S -> ERR CONFLICT");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_A_S, TRAFFIC_COLOR_RED) == TRAFFIC_RESULT_OK, "SET A_S RED");
    CHECK(all_red(&last), "sipka zhasne rovno (bez zltej)");
    run(TRAFFIC_ALL_RED_MS - 1U);
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_CONFLICT,
          "999 ms po zhasnuti este ERR CONFLICT");
    run(1);
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET B_D GREEN po 1 s");

    printf("F2e - SET B zapne oba B, SET B_R RED ukonci celu fazu\n");
    TrafficFsm_RequestSignal(TRAFFIC_TARGET_B_D, TRAFFIC_COLOR_RED);
    run(TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET B GREEN");
    expect_green(M_BR | M_BD, "oba B");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B_R, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK,
          "SET B_R GREEN pri zelenej B potvrdi");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B_R, TRAFFIC_COLOR_RED) == TRAFFIC_RESULT_OK, "SET B_R RED");
    CHECK(last.b_r == TRAFFIC_COLOR_YELLOW && last.b_d == TRAFFIC_COLOR_YELLOW, "oba B na zltu");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_B_R, TRAFFIC_COLOR_RED) == TRAFFIC_RESULT_OK,
          "SET RED pocas zltej potvrdi stav");
    run(TRAFFIC_YELLOW_MS - 1U);
    CHECK(last.b_r == TRAFFIC_COLOR_YELLOW, "zlta dobehne cela");

    printf("F2f - chodci v Control: tlacidlo -> P_REQ, SET P GREEN ho vybavi\n");
    run(1 + TRAFFIC_ALL_RED_MS);
    press_button();
    CHECK(status().p_req, "P_REQ=1");
    run(5000);
    CHECK(all_red(&last) && status().p_req, "v Control sa P samo nezapne");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_P, TRAFFIC_COLOR_YELLOW) == TRAFFIC_RESULT_ERR_STATE, "SET P YELLOW -> ERR STATE");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_P, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET P GREEN");
    CHECK(!status().p_req && last.p == TRAFFIC_COLOR_GREEN, "P zelena, P_REQ=0");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_P, TRAFFIC_COLOR_RED) == TRAFFIC_RESULT_OK, "SET P RED");
    CHECK(all_red(&last), "P prejde rovno na cervenu");

    printf("F2g - neplatne parametre\n");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_COUNT, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_PARAM, "neplatny ciel");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_A, (TrafficColor)7) == TRAFFIC_RESULT_ERR_PARAM, "neplatna farba");
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_NONE) == TRAFFIC_RESULT_ERR_PARAM, "neplatny rezim");
}

/* ---------------------------------------------------------------------------
 * F3 - Prepnutie rezimu
 * ------------------------------------------------------------------------- */

static void test_f3(void)
{
    printf("F3a - pocas B s C_S stlacit tlacidlo a prepnut do Control\n");
    verbose = 1;
    reset(0);
    car = true;
    run(TRAFFIC_ALL_RED_MS + 2U * 8000U + 1000U);
    expect_green(M_BR | M_BD | M_CS, "B s C_S");
    press_button();
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL) == TRAFFIC_RESULT_OK, "MODE CONTROL");
    CHECK(!last.c_s_on && last.b_r == TRAFFIC_COLOR_YELLOW && last.b_d == TRAFFIC_COLOR_YELLOW,
          "C_S zhasne hned, oba B na zltu");
    TrafficFsmStatus st = status();
    CHECK(st.mode == TRAFFIC_MODE_DEMO && st.next_mode == TRAFFIC_MODE_CONTROL && !st.ready,
          "pocas prepinania MODE=DEMO NEXT=CONTROL READY=0");
    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_P, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_ERR_BUSY,
          "SET pocas prepinania -> ERR BUSY");
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO) == TRAFFIC_RESULT_ERR_BUSY,
          "navrat do opustaneho rezimu -> ERR BUSY");
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL) == TRAFFIC_RESULT_OK,
          "opakovana poziadavka na ten isty ciel -> OK");
    run(TRAFFIC_YELLOW_MS);
    CHECK(all_red(&last), "po zltej spolocna cervena");
    CHECK(status().next_mode == TRAFFIC_MODE_CONTROL, "prepinanie trva do konca ochrannej cervenej");
    run(TRAFFIC_ALL_RED_MS);
    st = status();
    CHECK(st.mode == TRAFFIC_MODE_CONTROL && st.next_mode == TRAFFIC_MODE_NONE && st.ready && st.p_req,
          "MODE=CONTROL NEXT=- READY=1 P_REQ=1");
    CHECK(st.car_c, "CAR_C=1 vo vypise");
    run(10000);
    CHECK(all_red(&last), "Control caka pri spolocnej cervenej");

    CHECK(TrafficFsm_RequestSignal(TRAFFIC_TARGET_P, TRAFFIC_COLOR_GREEN) == TRAFFIC_RESULT_OK, "SET P GREEN");
    expect_green(M_P, "chodci");
    run(2000);
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO) == TRAFFIC_RESULT_OK, "MODE DEMO pocas zelenej P");
    CHECK(all_red(&last), "P prejde rovno na cervenu");
    run(TRAFFIC_ALL_RED_MS - 1U);
    CHECK(all_red(&last) && status().next_mode == TRAFFIC_MODE_DEMO, "ochranna sekunda");
    run(1);
    expect_green(M_A | M_BR, "po navrate do Demo zacne fazou A");
    CHECK(status().mode == TRAFFIC_MODE_DEMO, "MODE=DEMO");
    verbose = 0;

    printf("F3b - prepnutie pri uz uplynutej ochrannej sekunde je okamzite\n");
    reset(0);
    TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL);
    run(5000);
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO) == TRAFFIC_RESULT_OK, "MODE DEMO");
    CHECK(status().mode == TRAFFIC_MODE_DEMO && status().next_mode == TRAFFIC_MODE_NONE, "prepne hned");
    expect_green(M_A | M_BR, "A hned po prepnuti");
    CHECK(TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO) == TRAFFIC_RESULT_OK, "MODE DEMO v Demo potvrdi");

    printf("F3c - zmena rezimu pocas cestnej zelenej v Control (cez zltu)\n");
    TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL);
    run(TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    TrafficFsm_RequestSignal(TRAFFIC_TARGET_C, TRAFFIC_COLOR_GREEN);
    expect_green(M_C, "C");
    TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO);
    CHECK(last.c == TRAFFIC_COLOR_YELLOW, "C dobehne cez zltu");
    run(TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    expect_green(M_A | M_BR, "Demo od fazy A");

    printf("F3d - cakajuca poziadavka chodcov sa prenesie do Demo\n");
    reset(0);
    TrafficFsm_RequestMode(TRAFFIC_MODE_CONTROL);
    run(TRAFFIC_ALL_RED_MS);
    press_button();
    TrafficFsm_RequestMode(TRAFFIC_MODE_DEMO);
    expect_green(M_A | M_BR, "Demo zacne fazou A");
    CHECK(status().p_req, "P_REQ ostane");
    run(TRAFFIC_GREEN_MS + TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    expect_green(M_P, "chodci po prvej faze A");
}

/* ---------------------------------------------------------------------------
 * Dalsie kontroly
 * ------------------------------------------------------------------------- */

static void test_overflow(void)
{
    printf("Pretecenie milisekundoveho casu (uint32_t)\n");
    reset(0xFFFFFFFFU - 3000U);
    run(TRAFFIC_ALL_RED_MS);
    expect_green(M_A | M_BR, "A pred pretecenim");
    run(TRAFFIC_GREEN_MS + TRAFFIC_YELLOW_MS + TRAFFIC_ALL_RED_MS);
    CHECK(now < 10000U, "cas pretiekol");
    expect_green(M_BR | M_BD, "B po preteceni");
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

/* Dlha nahodna simulacia: vstupy aj prikazy nahodne, kontroluju sa pravidla v on_output */
static void test_random(void)
{
    printf("Nahodna simulacia 2 hodiny (vstupy aj prikazy), kontrola pravidiel pri kazdej zmene\n");
    srand(12345U);
    reset(0);
    const int start_failures = failures;
    for (uint32_t i = 0; i < 2U * 3600U * 1000U; i++)
    {
        if (rand() % 20000 == 0) car = !car;
        if (rand() % 15000 == 0) press = true;
        if (rand() % 4000 == 0)
        {
            const int r = rand() % 10;
            if (r == 0)
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
    printf("  zmien svetiel: %d, zapnutych zelenych: %d\n", changes, greens_started);
    CHECK(greens_started > 300, "simulacia presla mnoho faz");
}

int main(void)
{
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
