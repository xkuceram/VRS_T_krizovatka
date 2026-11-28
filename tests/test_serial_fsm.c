/*
 * Spolocny test protokolu USART so skutocnym automatom (skusky U1 - U3 na PC).
 *
 * Retazec je rovnaky ako na doske: znaky z terminalu -> traffic_serial ->
 * traffic_serial_link -> traffic_fsm. Cas sa simuluje, snimac C a tlacidlo
 * sa zadavaju priamo do TrafficFsm_Update.
 *
 *   gcc -std=c99 -Wall -Wextra -I../Core/Inc test_serial_fsm.c ../Core/Src/traffic_serial.c \
 *       ../Core/Src/traffic_serial_link.c ../Core/Src/traffic_fsm.c -o test_serial_fsm
 *   ./test_serial_fsm
 * alebo v priecinku tests: make
 */

#include <stdio.h>
#include <string.h>
#include "traffic_fsm.h"
#include "traffic_serial.h"
#include "traffic_serial_link.h"

static int failures = 0;
static int checks = 0;
static uint32_t now = 0;
static int car_c = 0;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("  FAIL t=%6u ms: ", (unsigned)now); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* posunie simulovany cas po 10 ms, ako hlavna slucka */
static void run_ms(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10U)
    {
        now += 10U;
        TrafficFsm_Update(now, car_c != 0, false);
    }
}

static void press_button(void)
{
    now += 10U;
    TrafficFsm_Update(now, car_c != 0, true);
}

/* posle riadok po znakoch a vrati odpoved bez CRLF */
static const char *send_line(const char *line)
{
    static char last[TRAFFIC_SERIAL_REPLY_MAX];
    last[0] = '\0';
    for (const char *p = line; *p != '\0'; p++)
    {
        char reply[TRAFFIC_SERIAL_REPLY_MAX];
        size_t n = TrafficSerial_PutChar(*p, reply, sizeof reply);
        if (n > 0U)
        {
            CHECK(n >= 2U && reply[n - 2U] == '\r' && reply[n - 1U] == '\n', "odpoved nekonci CRLF");
            reply[n - 2U] = '\0';
            strcpy(last, reply);
        }
    }
    return last;
}

static void print_line(const char *line)
{
    for (; *line != '\0'; line++)
    {
        if (*line == '\r')      printf("\\r");
        else if (*line == '\n') printf("\\n");
        else                    putchar(*line);
    }
}

/* posle prikaz a overi zaciatok odpovede */
static void expect(const char *line, const char *expected_prefix)
{
    const char *r = send_line(line);
    printf("  t=%6u ms  ", (unsigned)now);
    print_line(line);
    printf("  ->  %s\n", r);
    CHECK(strncmp(r, expected_prefix, strlen(expected_prefix)) == 0,
          "odpoved \"%s\", ocakavany zaciatok \"%s\"", r, expected_prefix);
}

/* overi, ze odpoved na STATUS obsahuje dany text */
static void expect_status_has(const char *part)
{
    const char *r = send_line("STATUS\n");
    CHECK(strstr(r, part) != NULL, "STATUS \"%s\" neobsahuje \"%s\"", r, part);
}

/* caka, kym STATUS ukaze dany text (najviac timeout_ms) */
static void wait_status(const char *part, uint32_t timeout_ms)
{
    uint32_t start = now;
    while (strstr(send_line("STATUS\n"), part) == NULL && (now - start) < timeout_ms)
    {
        run_ms(10);
    }
    printf("  t=%6u ms  (cakanie na %s)\n", (unsigned)now, part);
    expect_status_has(part);
}

static void start(void)
{
    now = 0;
    car_c = 0;
    TrafficFsm_RegisterOutputCallback(NULL);
    TrafficFsm_Init(now);
    TrafficSerial_Init(TrafficSerialLink_Handlers());
}

static void go_control(void)
{
    expect("MODE CONTROL\r\n", "OK");
    wait_status("MODE=CONTROL NEXT=- READY=1", 10000);
}

/* --------------------------------------------------------------------------
 * U1 - ovladanie a vypis stavu
 * ------------------------------------------------------------------------ */
static void test_u1(void)
{
    static const char *const targets[] = {"B_R", "B_D", "C", "C_S"};
    char cmd[32], field[16];

    printf("\nU1 - ovladanie a vypis stavu\n");
    start();
    run_ms(2000);
    go_control();

    for (size_t i = 0; i < sizeof targets / sizeof targets[0]; i++)
    {
        const int arrow = (strcmp(targets[i], "C_S") == 0);
        snprintf(cmd, sizeof cmd, "SET %s GREEN\r\n", targets[i]);
        expect(cmd, "OK");
        snprintf(field, sizeof field, arrow ? " %s=ON" : " %s=GREEN", targets[i]);
        expect_status_has(field);
        printf("  t=%6u ms  STATUS             ->  %s\n", (unsigned)now, send_line("STATUS\n"));

        snprintf(cmd, sizeof cmd, "SET %s RED\n", targets[i]);
        expect(cmd, "OK");
        wait_status("READY=1", 10000);
    }

    printf("  STATUS s LF aj CRLF, auto na C a cakajuci chodec\n");
    car_c = 1;
    run_ms(200);
    press_button();
    run_ms(100);
    expect("STATUS\n", "OK MODE=CONTROL");
    expect_status_has("CAR_C=1");
    expect_status_has("P_REQ=1");
    expect("STATUS\r\n", "OK MODE=CONTROL");
    CHECK(strstr(send_line("STATUS\r\n"), "CAR_C=1 P_REQ=1") != NULL, "CRLF: chyba CAR_C=1 P_REQ=1");

    printf("  zelena pre P vybavi poziadavku chodcov\n");
    expect("SET P GREEN\n", "OK");
    expect_status_has("P=GREEN");
    expect_status_has("P_REQ=0");
}

/* --------------------------------------------------------------------------
 * U2 - chybny prikaz a obnova prijmu
 * ------------------------------------------------------------------------ */
static void test_u2(void)
{
    char line33[40];

    printf("\nU2 - chybny prikaz a obnova prijmu\n");
    start();
    run_ms(2000);
    go_control();

    memset(line33, 'X', 33);
    strcpy(line33 + 33, "\r\n");
    expect("SET X GREEN\r\n", "ERR SYNTAX");
    expect("SET A_S YELLOW\r\n", "ERR STATE");
    expect(line33, "ERR LENGTH");
    expect("STATUS\r\n", "OK MODE=CONTROL NEXT=- READY=1");
}

/* --------------------------------------------------------------------------
 * U3 - prikazy pocas behu
 * ------------------------------------------------------------------------ */
static void test_u3(void)
{
    printf("\nU3 - prikazy pocas behu\n");
    start();
    run_ms(2000);                                   /* Demo, prva faza svieti */
    expect("STATUS\n", "OK MODE=DEMO NEXT=-");
    expect("SET A RED\n", "ERR MODE");
    expect_status_has("A=GREEN");                   /* cyklus pokracuje */

    expect("MODE CONTROL\n", "OK");
    expect("SET P GREEN\n", "ERR BUSY");
    expect_status_has("NEXT=CONTROL");
    expect("MODE CONTROL\n", "OK");                 /* opakovanie toho isteho ciela */
    expect("MODE DEMO\n", "ERR BUSY");              /* navrat do opustaneho rezimu */
    wait_status("MODE=CONTROL NEXT=- READY=1", 10000);
    expect("SET A GREEN\n", "OK");
}

int main(void)
{
    printf("Spolocny test protokolu USART a automatu (U1 - U3)\n");
    test_u1();
    test_u2();
    test_u3();
    printf("\nVysledok: %d kontrol, %d chyb\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
