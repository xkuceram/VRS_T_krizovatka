#include "traffic_serial.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Stav prijimaca riadkov
 * ------------------------------------------------------------------------- */

static const TrafficSerialHandlers *serial_handlers = NULL;

static char rx_line[TRAFFIC_SERIAL_LINE_MAX + 1U];
static size_t rx_len = 0;
static uint8_t rx_overflow = 0;   /* riadok prekrocil 32 znakov, zvysok sa zahadzuje */
static uint8_t rx_invalid = 0;    /* v riadku bol nulovy bajt 0x00 -> ERR SYNTAX */
static uint8_t last_was_cr = 0;   /* CR uz ukoncil riadok, nasledujuci LF patri k nemu */

/* ---------------------------------------------------------------------------
 * Texty protokolu
 * ------------------------------------------------------------------------- */

static const char *const target_names[SERIAL_TARGET_COUNT] =
{
    [SERIAL_TARGET_A]   = "A",
    [SERIAL_TARGET_A_S] = "A_S",
    [SERIAL_TARGET_B_R] = "B_R",
    [SERIAL_TARGET_B_D] = "B_D",
    [SERIAL_TARGET_C]   = "C",
    [SERIAL_TARGET_C_S] = "C_S",
    [SERIAL_TARGET_P]   = "P",
    [SERIAL_TARGET_B]   = "B"
};

static const char *color_text(SerialColor color)
{
    switch (color)
    {
        case SERIAL_COLOR_GREEN:  return "GREEN";
        case SERIAL_COLOR_YELLOW: return "YELLOW";
        default:                  return "RED";
    }
}

static const char *mode_text(SerialMode mode)
{
    switch (mode)
    {
        case SERIAL_MODE_DEMO:    return "DEMO";
        case SERIAL_MODE_CONTROL: return "CONTROL";
        default:                  return "-";
    }
}

const char *TrafficSerial_ResultText(SerialResult result)
{
    switch (result)
    {
        case SERIAL_RESULT_OK:           return "OK";
        case SERIAL_RESULT_ERR_BUSY:     return "ERR BUSY";
        case SERIAL_RESULT_ERR_MODE:     return "ERR MODE";
        case SERIAL_RESULT_ERR_STATE:    return "ERR STATE";
        case SERIAL_RESULT_ERR_CONFLICT: return "ERR CONFLICT";
        case SERIAL_RESULT_ERR_SYNTAX:   return "ERR SYNTAX";
        default:                         return "ERR BUSY";
    }
}

/* ---------------------------------------------------------------------------
 * Skladanie odpovede do buffera (bez snprintf, s kontrolou dlzky)
 * ------------------------------------------------------------------------- */

typedef struct
{
    char *buf;
    size_t size;
    size_t len;
} TextOut;

static void out_str(TextOut *o, const char *s)
{
    while (*s != '\0' && (o->len + 1U) < o->size)
    {
        o->buf[o->len++] = *s++;
    }
    o->buf[o->len] = '\0';
}

static void out_field(TextOut *o, const char *name, const char *value)
{
    out_str(o, " ");
    out_str(o, name);
    out_str(o, "=");
    out_str(o, value);
}

size_t TrafficSerial_FormatStatus(const SerialStatus *st, char *out, size_t out_size)
{
    TextOut o = {out, out_size, 0};
    if (out_size == 0U)
    {
        return 0;
    }
    out[0] = '\0';

    out_str(&o, "OK");
    out_field(&o, "MODE",  mode_text(st->mode));
    out_field(&o, "NEXT",  mode_text(st->next));
    out_field(&o, "READY", st->ready ? "1" : "0");
    out_field(&o, "A",     color_text(st->a));
    out_field(&o, "A_S",   st->a_s ? "ON" : "OFF");
    out_field(&o, "B_R",   color_text(st->b_r));
    out_field(&o, "B_D",   color_text(st->b_d));
    out_field(&o, "C",     color_text(st->c));
    out_field(&o, "C_S",   st->c_s ? "ON" : "OFF");
    out_field(&o, "P",     color_text(st->p));
    out_field(&o, "CAR_C", st->car_c ? "1" : "0");
    out_field(&o, "P_REQ", st->p_req ? "1" : "0");
    return o.len;
}

/* ---------------------------------------------------------------------------
 * Rozbor prikazu
 * ------------------------------------------------------------------------- */

#define MAX_TOKENS 3U

/* Rozdeli riadok podla medzier. Polozky oddeluje prave jedna medzera. */
static int split_tokens(char *line, char *tokens[], size_t *count)
{
    size_t n = 0;
    char *p = line;

    if (*p == '\0' || *p == ' ')
    {
        return 0;                       /* medzera na zaciatku */
    }
    while (1)
    {
        if (n == MAX_TOKENS)
        {
            return 0;                   /* privela poloziek */
        }
        tokens[n++] = p;
        while (*p != '\0' && *p != ' ')
        {
            p++;
        }
        if (*p == '\0')
        {
            break;
        }
        *p++ = '\0';                    /* ukonci polozku na mieste medzery */
        if (*p == '\0' || *p == ' ')
        {
            return 0;                   /* medzera na konci alebo dve medzery za sebou */
        }
    }
    *count = n;
    return 1;
}

static int parse_target(const char *s, SerialTarget *target)
{
    for (size_t i = 0; i < (size_t)SERIAL_TARGET_COUNT; i++)
    {
        if (strcmp(s, target_names[i]) == 0)
        {
            *target = (SerialTarget)i;
            return 1;
        }
    }
    return 0;
}

static int parse_color(const char *s, SerialColor *color)
{
    if (strcmp(s, "GREEN") == 0)  { *color = SERIAL_COLOR_GREEN;  return 1; }
    if (strcmp(s, "YELLOW") == 0) { *color = SERIAL_COLOR_YELLOW; return 1; }
    if (strcmp(s, "RED") == 0)    { *color = SERIAL_COLOR_RED;    return 1; }
    return 0;
}

int TrafficSerial_Parse(const char *line, SerialCommand *cmd)
{
    char copy[TRAFFIC_SERIAL_LINE_MAX + 1U];
    char *tok[MAX_TOKENS];
    size_t n = 0;

    if (strlen(line) > TRAFFIC_SERIAL_LINE_MAX)
    {
        return 0;
    }
    strcpy(copy, line);
    if (!split_tokens(copy, tok, &n))
    {
        return 0;
    }

    if (n == 1U && strcmp(tok[0], "STATUS") == 0)
    {
        cmd->type = SERIAL_CMD_STATUS;
        return 1;
    }
    if (n == 2U && strcmp(tok[0], "MODE") == 0)
    {
        cmd->type = SERIAL_CMD_MODE;
        if (strcmp(tok[1], "DEMO") == 0)    { cmd->mode = SERIAL_MODE_DEMO;    return 1; }
        if (strcmp(tok[1], "CONTROL") == 0) { cmd->mode = SERIAL_MODE_CONTROL; return 1; }
        return 0;
    }
    if (n == 3U && strcmp(tok[0], "SET") == 0)
    {
        cmd->type = SERIAL_CMD_SET;
        return parse_target(tok[1], &cmd->target) && parse_color(tok[2], &cmd->color);
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Spracovanie celeho riadka
 * ------------------------------------------------------------------------- */

static void handle_line(const char *line, TextOut *o)
{
    SerialCommand cmd;
    const TrafficSerialHandlers *h = serial_handlers;

    if (!TrafficSerial_Parse(line, &cmd))
    {
        out_str(o, "ERR SYNTAX");
        return;
    }

    /* kontrola registracie funkcii automatu (NULL) pred ich volanim */
    switch (cmd.type)
    {
        case SERIAL_CMD_STATUS:
            if (h != NULL && h->get_status != NULL)
            {
                SerialStatus st;
                memset(&st, 0, sizeof st);
                h->get_status(&st);
                o->len += TrafficSerial_FormatStatus(&st, o->buf + o->len, o->size - o->len);
            }
            else
            {
                out_str(o, "ERR BUSY");
            }
            break;

        case SERIAL_CMD_MODE:
            out_str(o, (h != NULL && h->request_mode != NULL)
                       ? TrafficSerial_ResultText(h->request_mode(cmd.mode))
                       : "ERR BUSY");
            break;

        case SERIAL_CMD_SET:
            out_str(o, (h != NULL && h->request_signal != NULL)
                       ? TrafficSerial_ResultText(h->request_signal(cmd.target, cmd.color))
                       : "ERR BUSY");
            break;

        default:
            out_str(o, "ERR SYNTAX");
            break;
    }
}

void TrafficSerial_Init(const TrafficSerialHandlers *handlers)
{
    serial_handlers = handlers;
    rx_len = 0;
    rx_overflow = 0;
    rx_invalid = 0;
    last_was_cr = 0;
}

size_t TrafficSerial_PutChar(char c, char *reply, size_t reply_size)
{
    TextOut o = {reply, reply_size, 0};

    if (reply != NULL && reply_size > 0U)
    {
        reply[0] = '\0';
    }

    /* LF hned po CR patri k tomu istemu koncu riadka (CRLF) */
    if (c == '\n' && last_was_cr)
    {
        last_was_cr = 0;
        return 0;
    }
    last_was_cr = (c == '\r');

    if (c != '\r' && c != '\n')
    {
        if (c == '\0')
        {
            rx_invalid = 1;             /* C retazec by sa tu skoncil, riadok sa nevykona */
        }
        if (rx_len < TRAFFIC_SERIAL_LINE_MAX)
        {
            rx_line[rx_len++] = c;
        }
        else
        {
            rx_overflow = 1;            /* 33. znak a dalsie sa zahadzuju */
        }
        return 0;
    }

    /* koniec riadka */
    if (reply == NULL || reply_size < TRAFFIC_SERIAL_REPLY_MAX)
    {
        rx_len = 0;                     /* buffer je mensi ako minimum: riadok sa zahodi */
        rx_overflow = 0;                /* skor, nez by sa zavolal automat */
        rx_invalid = 0;
        return 0;
    }
    if (rx_overflow)
    {
        out_str(&o, "ERR LENGTH");
    }
    else if (rx_len == 0U)
    {
        return 0;                       /* prazdny riadok sa preskoci */
    }
    else if (rx_invalid)
    {
        out_str(&o, "ERR SYNTAX");      /* nulovy bajt v riadku, automat sa nevola */
    }
    else
    {
        rx_line[rx_len] = '\0';
        o.size = reply_size - 2U;       /* miesto na CRLF */
        handle_line(rx_line, &o);
        o.size = reply_size;
    }
    rx_len = 0;
    rx_overflow = 0;
    rx_invalid = 0;

    out_str(&o, "\r\n");
    return o.len;
}
