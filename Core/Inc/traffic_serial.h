#ifndef TRAFFIC_SERIAL_H
#define TRAFFIC_SERIAL_H

/*
 * Komunikacny protokol T-krizovatky (vetva feature/usart-control).
 *
 * Modul je ciste C bez HAL. Dostava prijate znaky po jednom a vracia hotovu
 * odpoved (riadok s CRLF). Neprijima ani neodosiela bajty - to robi
 * hardverova vrstva (serial_port.c) cez prerusenia. Vdaka tomu sa da cely
 * protokol otestovat na PC (tests/test_traffic_serial.c).
 *
 * S automatom komunikuje cez zaregistrovane funkcie (TrafficSerialHandlers),
 * podobne ako automat oznamuje fazu ovladacu GPIO cez callback. Protokol
 * preto nezavisi od toho, ako je automat vnutorne napisany.
 *
 * Pouzitie v hlavnej slucke:
 *   TrafficSerial_Init(&handlers);
 *   while (1) {
 *       while (serial_port_read(&c)) {
 *           char reply[TRAFFIC_SERIAL_REPLY_MAX];
 *           size_t n = TrafficSerial_PutChar(c, reply, sizeof reply);
 *           if (n > 0) serial_port_write(reply, n);
 *       }
 *   }
 */

#include <stddef.h>
#include <stdint.h>

#define TRAFFIC_SERIAL_LINE_MAX   32U   /* max. dlzka prikazu bez LF / CRLF */
#define TRAFFIC_SERIAL_REPLY_MAX  128U  /* odpoved STATUS ma ~100 znakov + CRLF */

/* Rezim */
typedef enum
{
    SERIAL_MODE_DEMO = 0,
    SERIAL_MODE_CONTROL,
    SERIAL_MODE_NONE                    /* NEXT=- (neprebieha prepinanie) */
} SerialMode;

/* Ciel prikazu SET */
typedef enum
{
    SERIAL_TARGET_A = 0,
    SERIAL_TARGET_A_S,
    SERIAL_TARGET_B_R,
    SERIAL_TARGET_B_D,
    SERIAL_TARGET_C,
    SERIAL_TARGET_C_S,
    SERIAL_TARGET_P,
    SERIAL_TARGET_B,                    /* oba semafory sprava (B_R aj B_D) */
    SERIAL_TARGET_COUNT
} SerialTarget;

/* Farba v prikaze SET aj vo vypise STATUS */
typedef enum
{
    SERIAL_COLOR_RED = 0,
    SERIAL_COLOR_YELLOW,
    SERIAL_COLOR_GREEN
} SerialColor;

/* Vysledok poziadavky od automatu */
typedef enum
{
    SERIAL_RESULT_OK = 0,
    SERIAL_RESULT_ERR_BUSY,
    SERIAL_RESULT_ERR_MODE,
    SERIAL_RESULT_ERR_STATE,
    SERIAL_RESULT_ERR_CONFLICT
} SerialResult;

/* Udaje pre vypis STATUS (vyplni automat) */
typedef struct
{
    SerialMode mode;                    /* aktivny rezim */
    SerialMode next;                    /* cielovy rezim pocas prepinania, inak SERIAL_MODE_NONE */
    uint8_t ready;                      /* 1 = Control moze prijat novu zelenu */
    SerialColor a, b_r, b_d, c, p;      /* farby beznych semaforov */
    uint8_t a_s, c_s;                   /* sipky: 1 = ON, 0 = OFF */
    uint8_t car_c;                      /* 1 = obsadeny vjazd C */
    uint8_t p_req;                      /* 1 = caka poziadavka chodcov */
} SerialStatus;

/* Funkcie automatu, ktore protokol vola (navrh rozhrania pre traffic_fsm) */
typedef struct
{
    SerialResult (*request_mode)(SerialMode mode);                        /* MODE DEMO / MODE CONTROL */
    SerialResult (*request_signal)(SerialTarget target, SerialColor color); /* SET <ciel> <farba> */
    void (*get_status)(SerialStatus *status);                             /* STATUS */
} TrafficSerialHandlers;

/* Rozobraty prikaz */
typedef enum
{
    SERIAL_CMD_MODE = 0,
    SERIAL_CMD_SET,
    SERIAL_CMD_STATUS
} SerialCommandType;

typedef struct
{
    SerialCommandType type;
    SerialMode mode;                    /* pri MODE */
    SerialTarget target;                /* pri SET */
    SerialColor color;                  /* pri SET */
} SerialCommand;

/* Registracia funkcii automatu a vynulovanie prijimaca. Volat pred povolenim prijmu. */
void TrafficSerial_Init(const TrafficSerialHandlers *handlers);

/*
 * Spracuje jeden prijaty znak. Ak tym skoncil neprazdny riadok, zapise do
 * reply odpoved ukoncenu CRLF a vrati jej dlzku. Inak vrati 0.
 */
size_t TrafficSerial_PutChar(char c, char *reply, size_t reply_size);

/* Pomocne funkcie (pouzite vo vnutri, verejne kvoli testom) */
int TrafficSerial_Parse(const char *line, SerialCommand *cmd);          /* 1 = platny prikaz, 0 = ERR SYNTAX */
size_t TrafficSerial_FormatStatus(const SerialStatus *status, char *out, size_t out_size);
const char *TrafficSerial_ResultText(SerialResult result);

#endif
