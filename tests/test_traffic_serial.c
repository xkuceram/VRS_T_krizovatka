/*
 * Testy komunikacneho protokolu (modul traffic_serial) na PC bez dosky.
 *
 * Automat je nahradeny jednoduchou nahradou (fake), ktora si zapamata
 * poslednu poziadavku a vrati vopred nastaveny vysledok. Overuje sa
 * skladanie riadkov, kontrola syntaxe a presny text odpovedi.
 *
 *   gcc -std=c99 -Wall -Wextra -I../Core/Inc test_traffic_serial.c ../Core/Src/traffic_serial.c -o test_traffic_serial
 *   ./test_traffic_serial
 * alebo v priecinku tests: make
 */

#include <stdio.h>
#include <string.h>
#include "traffic_serial.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* --------------------------------------------------------------------------
 * Nahrada automatu
 * ------------------------------------------------------------------------ */

static SerialResult fake_result = SERIAL_RESULT_OK;
static int calls_mode = 0, calls_signal = 0, calls_status = 0;
static SerialMode last_mode;
static SerialTarget last_target;
static SerialColor last_color;
static SerialStatus fake_status;

static SerialResult fake_request_mode(SerialMode mode)
{
    calls_mode++;
    last_mode = mode;
    return fake_result;
}

static SerialResult fake_request_signal(SerialTarget target, SerialColor color)
{
    calls_signal++;
    last_target = target;
    last_color = color;
    return fake_result;
}

static void fake_get_status(SerialStatus *st)
{
    calls_status++;
    *st = fake_status;
}

static const TrafficSerialHandlers fake_handlers =
{
    fake_request_mode,
    fake_request_signal,
    fake_get_status
};

static void reset_fake(void)
{
    fake_result = SERIAL_RESULT_OK;
    calls_mode = calls_signal = calls_status = 0;
    memset(&fake_status, 0, sizeof fake_status);
    fake_status.mode = SERIAL_MODE_CONTROL;
    fake_status.next = SERIAL_MODE_NONE;
    fake_status.ready = 1;
}

/* --------------------------------------------------------------------------
 * Pomocne funkcie: posle text po znakoch a zozbiera vsetky odpovede
 * ------------------------------------------------------------------------ */

static char replies[1024];
static int reply_count;

/* posle presne len bajtov (aj nulovy bajt 0x00, na ktorom by sa C retazec skoncil) */
static void send_bytes(const char *data, size_t len)
{
    replies[0] = '\0';
    reply_count = 0;
    for (size_t i = 0; i < len; i++)
    {
        char reply[TRAFFIC_SERIAL_REPLY_MAX];
        size_t n = TrafficSerial_PutChar(data[i], reply, sizeof reply);
        if (n > 0U)
        {
            CHECK(n == strlen(reply), "dlzka odpovede %u nesedi s textom", (unsigned)n);
            CHECK(n < TRAFFIC_SERIAL_REPLY_MAX, "odpoved je pridlha (%u)", (unsigned)n);
            strncat(replies, reply, sizeof replies - strlen(replies) - 1U);
            reply_count++;
        }
    }
}

static void send(const char *text)
{
    send_bytes(text, strlen(text));
}

static void print_escaped_n(const char *s, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (s[i] == '\r')      printf("\\r");
        else if (s[i] == '\n') printf("\\n");
        else if (s[i] == '\0') printf("\\0");
        else                   putchar(s[i]);
    }
}

static void print_escaped(const char *s)
{
    print_escaped_n(s, strlen(s));
}

/* posle text a overi, ze prisli presne ocakavane odpovede (spojene za sebou) */
static void expect(const char *text, const char *expected)
{
    send(text);
    printf("  ");
    print_escaped(text);
    printf("  ->  ");
    print_escaped(replies[0] ? replies : "(bez odpovede)");
    printf("\n");
    CHECK(strcmp(replies, expected) == 0, "ocakavane \"%s\"", expected);
}

/* to iste ako expect, ale s explicitnou dlzkou vstupu */
static void expect_bytes(const char *data, size_t len, const char *expected)
{
    send_bytes(data, len);
    printf("  ");
    print_escaped_n(data, len);
    printf("  ->  ");
    print_escaped(replies[0] ? replies : "(bez odpovede)");
    printf("\n");
    CHECK(strcmp(replies, expected) == 0, "ocakavane \"%s\"", expected);
}

/* --------------------------------------------------------------------------
 * Testy
 * ------------------------------------------------------------------------ */

static void test_line_endings(void)
{
    printf("\nUkoncenie riadkov a prazdne riadky\n");
    reset_fake();
    expect("STATUS\n", "OK MODE=CONTROL NEXT=- READY=1 A=RED A_S=OFF B_R=RED B_D=RED C=RED C_S=OFF P=RED CAR_C=0 P_REQ=0\r\n");
    expect("STATUS\r\n", "OK MODE=CONTROL NEXT=- READY=1 A=RED A_S=OFF B_R=RED B_D=RED C=RED C_S=OFF P=RED CAR_C=0 P_REQ=0\r\n");
    expect("STATUS\r", "OK MODE=CONTROL NEXT=- READY=1 A=RED A_S=OFF B_R=RED B_D=RED C=RED C_S=OFF P=RED CAR_C=0 P_REQ=0\r\n");
    expect("\n\r\n\n", "");
    CHECK(reply_count == 0, "prazdne riadky nemaju dostat odpoved");
    expect("MODE DEMO\r\nMODE CONTROL\n", "OK\r\nOK\r\n");
    CHECK(reply_count == 2, "dva prikazy maju dostat dve odpovede, prislo %d", reply_count);
}

static void test_status_format(void)
{
    printf("\nVypis STATUS podla prikladu v README\n");
    reset_fake();
    fake_status.car_c = 1;
    fake_status.p_req = 1;
    expect("STATUS\r\n", "OK MODE=CONTROL NEXT=- READY=1 A=RED A_S=OFF B_R=RED B_D=RED C=RED C_S=OFF P=RED CAR_C=1 P_REQ=1\r\n");

    fake_status.mode = SERIAL_MODE_DEMO;
    fake_status.next = SERIAL_MODE_CONTROL;
    fake_status.ready = 0;
    fake_status.a = SERIAL_COLOR_RED;
    fake_status.b_r = SERIAL_COLOR_YELLOW;
    fake_status.b_d = SERIAL_COLOR_YELLOW;
    fake_status.c_s = 0;
    fake_status.car_c = 0;
    fake_status.p_req = 0;
    expect("STATUS\n", "OK MODE=DEMO NEXT=CONTROL READY=0 A=RED A_S=OFF B_R=YELLOW B_D=YELLOW C=RED C_S=OFF P=RED CAR_C=0 P_REQ=0\r\n");

    fake_status.mode = SERIAL_MODE_CONTROL;
    fake_status.next = SERIAL_MODE_NONE;
    fake_status.a = SERIAL_COLOR_RED;
    fake_status.a_s = 1;
    fake_status.b_r = SERIAL_COLOR_RED;
    fake_status.b_d = SERIAL_COLOR_RED;
    fake_status.c = SERIAL_COLOR_GREEN;
    expect("STATUS\n", "OK MODE=CONTROL NEXT=- READY=0 A=RED A_S=ON B_R=RED B_D=RED C=GREEN C_S=OFF P=RED CAR_C=0 P_REQ=0\r\n");
}

static void test_commands_forwarded(void)
{
    static const struct { const char *text; SerialTarget target; SerialColor color; } sets[] =
    {
        {"SET A GREEN\n",    SERIAL_TARGET_A,   SERIAL_COLOR_GREEN},
        {"SET A_S GREEN\n",  SERIAL_TARGET_A_S, SERIAL_COLOR_GREEN},
        {"SET B_R YELLOW\n", SERIAL_TARGET_B_R, SERIAL_COLOR_YELLOW},
        {"SET B_D RED\n",    SERIAL_TARGET_B_D, SERIAL_COLOR_RED},
        {"SET C GREEN\n",    SERIAL_TARGET_C,   SERIAL_COLOR_GREEN},
        {"SET C_S GREEN\n",  SERIAL_TARGET_C_S, SERIAL_COLOR_GREEN},
        {"SET P GREEN\n",    SERIAL_TARGET_P,   SERIAL_COLOR_GREEN},
        {"SET B GREEN\n",    SERIAL_TARGET_B,   SERIAL_COLOR_GREEN},
    };

    printf("\nPlatne prikazy sa odovzdaju automatu\n");
    reset_fake();
    expect("MODE CONTROL\n", "OK\r\n");
    CHECK(calls_mode == 1 && last_mode == SERIAL_MODE_CONTROL, "MODE CONTROL sa neodovzdal automatu");
    expect("MODE DEMO\n", "OK\r\n");
    CHECK(calls_mode == 2 && last_mode == SERIAL_MODE_DEMO, "MODE DEMO sa neodovzdal automatu");

    for (size_t i = 0; i < sizeof sets / sizeof sets[0]; i++)
    {
        int before = calls_signal;
        expect(sets[i].text, "OK\r\n");
        CHECK(calls_signal == before + 1 && last_target == sets[i].target && last_color == sets[i].color,
              "%s sa neodovzdal so spravnym cielom a farbou", sets[i].text);
    }
}

static void test_results(void)
{
    printf("\nVysledky automatu sa prelozia na odpovede\n");
    reset_fake();
    fake_result = SERIAL_RESULT_ERR_BUSY;     expect("SET P GREEN\n", "ERR BUSY\r\n");
    fake_result = SERIAL_RESULT_ERR_MODE;     expect("SET A RED\n", "ERR MODE\r\n");
    fake_result = SERIAL_RESULT_ERR_STATE;    expect("SET A_S YELLOW\n", "ERR STATE\r\n");
    fake_result = SERIAL_RESULT_ERR_CONFLICT; expect("SET B_D GREEN\n", "ERR CONFLICT\r\n");
    fake_result = SERIAL_RESULT_ERR_BUSY;     expect("MODE DEMO\n", "ERR BUSY\r\n");
}

static void test_syntax(void)
{
    static const char *const bad[] =
    {
        "SET X GREEN\n", "set A GREEN\n", "SET A BLUE\n", "SET A\n", "SET A GREEN NOW\n",
        "SET  A GREEN\n", "SET A GREEN \n", " STATUS\n", "STATUS NOW\n", "MODE AUTO\n",
        "MODE\n", "HELLO\n", "SET A_SGREEN\n", "SET B_ GREEN\n", "SET A\tGREEN\n"
    };

    printf("\nChybna syntax -> ERR SYNTAX, automat sa nevola\n");
    reset_fake();
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        expect(bad[i], "ERR SYNTAX\r\n");
    }
    CHECK(calls_mode == 0 && calls_signal == 0 && calls_status == 0, "pri chybnej syntaxi sa nema volat automat");
}

static void test_length(void)
{
    char line32[40], line33[40], line60[70];

    printf("\nDlzka riadka (limit 32 znakov)\n");
    reset_fake();

    /* 32 znakov je este povolenych - pride ERR SYNTAX, nie ERR LENGTH */
    memset(line32, 'A', 32); strcpy(line32 + 32, "\n");
    expect(line32, "ERR SYNTAX\r\n");

    /* 33 znakov -> ERR LENGTH, a hned potom normalne spracovany STATUS */
    memset(line33, 'A', 33); strcpy(line33 + 33, "\r\n");
    expect(line33, "ERR LENGTH\r\n");
    expect("STATUS\r\n", "OK MODE=CONTROL NEXT=- READY=1 A=RED A_S=OFF B_R=RED B_D=RED C=RED C_S=OFF P=RED CAR_C=0 P_REQ=0\r\n");

    /* dlhy riadok, ktory vyzera ako platny prikaz, sa tiez zahodi cely */
    strcpy(line60, "SET A GREEN SET A GREEN SET A GREEN SET A GREEN SET A\n");
    expect(line60, "ERR LENGTH\r\n");
    CHECK(calls_signal == 0, "zahodeny riadok sa nema odovzdat automatu");
}

static void test_skuska_u2(void)
{
    char line33[40];

    printf("\nSkuska U2 - chybny prikaz a obnova prijmu (v ustalenom Control)\n");
    reset_fake();
    memset(line33, 'X', 33); strcpy(line33 + 33, "\r\n");

    expect("SET X GREEN\r\n", "ERR SYNTAX\r\n");
    fake_result = SERIAL_RESULT_ERR_STATE;      /* automat: A_S nie je bezny zeleny semafor */
    expect("SET A_S YELLOW\r\n", "ERR STATE\r\n");
    fake_result = SERIAL_RESULT_OK;
    expect(line33, "ERR LENGTH\r\n");
    expect("STATUS\r\n", "OK MODE=CONTROL NEXT=- READY=1 A=RED A_S=OFF B_R=RED B_D=RED C=RED C_S=OFF P=RED CAR_C=0 P_REQ=0\r\n");
}

static void test_nul_byte(void)
{
    static const char set_nul[]    = "SET A GREEN\0X\n";   /* pred 0x00 je platny prikaz */
    static const char status_nul[] = "STATUS\0\r\n";
    static const char only_nul[]   = "\0\n";
    static const char mode_nul[]   = "MODE\0DEMO\n";

    printf("\nNulovy bajt 0x00 v riadku -> ERR SYNTAX, automat sa nevola\n");
    reset_fake();
    expect_bytes(set_nul, sizeof set_nul - 1U, "ERR SYNTAX\r\n");
    expect_bytes(status_nul, sizeof status_nul - 1U, "ERR SYNTAX\r\n");
    expect_bytes(only_nul, sizeof only_nul - 1U, "ERR SYNTAX\r\n");
    expect_bytes(mode_nul, sizeof mode_nul - 1U, "ERR SYNTAX\r\n");
    CHECK(calls_mode == 0 && calls_signal == 0 && calls_status == 0,
          "riadok s nulovym bajtom sa nema odovzdat automatu");

    /* dalsi riadok sa uz spracuje normalne */
    expect("SET A GREEN\n", "OK\r\n");
    CHECK(calls_signal == 1, "po riadku s 0x00 sa ma dalsi platny prikaz vykonat");
}

static void test_no_handlers(void)
{
    printf("\nBez zaregistrovaneho automatu (NULL)\n");
    TrafficSerial_Init(NULL);
    expect("STATUS\n", "ERR BUSY\r\n");
    expect("SET A GREEN\n", "ERR BUSY\r\n");
    expect("SET X GREEN\n", "ERR SYNTAX\r\n");
    TrafficSerial_Init(&fake_handlers);
}

/* posle text s buffrom zadanej velkosti, vrati sucet dlzok odpovedi */
static size_t send_with_buffer(const char *text, char *reply, size_t reply_size)
{
    size_t n = 0;
    for (const char *p = text; *p != '\0'; p++)
    {
        n += TrafficSerial_PutChar(*p, reply, reply_size);
    }
    return n;
}

static void test_small_buffer(void)
{
    char reply[TRAFFIC_SERIAL_REPLY_MAX];

    printf("\nBuffer mensi ako TRAFFIC_SERIAL_REPLY_MAX -> bez odpovede a bez volania automatu\n");
    reset_fake();
    TrafficSerial_Init(&fake_handlers);

    CHECK(send_with_buffer("SET A GREEN\n", reply, sizeof reply - 1U) == 0, "pri mensom buffri sa nema nic zapisat");
    CHECK(send_with_buffer("STATUS\n", reply, 2U) == 0, "pri buffri 2 B sa nema nic zapisat");
    CHECK(send_with_buffer("MODE DEMO\n", NULL, 0U) == 0, "pri NULL buffri sa nema nic zapisat");
    CHECK(calls_mode == 0 && calls_signal == 0 && calls_status == 0,
          "pri malom buffri sa automat nema volat");

    /* s buffrom presne TRAFFIC_SERIAL_REPLY_MAX sa odpovie normalne */
    CHECK(send_with_buffer("SET A GREEN\n", reply, sizeof reply) == 4U && strcmp(reply, "OK\r\n") == 0,
          "pri buffri TRAFFIC_SERIAL_REPLY_MAX ma prist OK");
    printf("  ok\n");
    TrafficSerial_Init(&fake_handlers);
}

int main(void)
{
    printf("Testy protokolu USART (traffic_serial)\n");
    TrafficSerial_Init(&fake_handlers);

    test_line_endings();
    test_status_format();
    test_commands_forwarded();
    test_results();
    test_syntax();
    test_length();
    test_skuska_u2();
    test_nul_byte();
    test_no_handlers();
    test_small_buffer();

    printf("\nVysledok: %d kontrol, %d chyb\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
