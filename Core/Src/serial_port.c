#include "serial_port.h"

#if (SERIAL_PORT_RX_SIZE & (SERIAL_PORT_RX_SIZE - 1U)) != 0U || (SERIAL_PORT_TX_SIZE & (SERIAL_PORT_TX_SIZE - 1U)) != 0U
#error "Velkosti bufferov musia byt mocniny 2"
#endif

static UART_HandleTypeDef *port_uart = NULL;

/* ---------------------------------------------------------------------------
 * Prijem: prerusenie zapisuje rx_head, hlavna slucka zapisuje rx_tail
 * ------------------------------------------------------------------------- */
static uint8_t rx_buf[SERIAL_PORT_RX_SIZE];
static volatile uint32_t rx_head = 0;      /* sem zapise dalsi bajt prerusenie */
static volatile uint32_t rx_tail = 0;      /* odtialto cita hlavna slucka */
static volatile uint32_t rx_dropped = 0;
static uint8_t rx_byte;                    /* sem HAL prijme jeden bajt */

/* ---------------------------------------------------------------------------
 * Odosielanie: hlavna slucka zapisuje tx_head, prerusenie posuva tx_tail
 * ------------------------------------------------------------------------- */
static uint8_t tx_buf[SERIAL_PORT_TX_SIZE];
static volatile uint32_t tx_head = 0;      /* sem zapisuje hlavna slucka */
static volatile uint32_t tx_tail = 0;      /* odtialto sa odosiela */
static volatile uint16_t tx_chunk = 0;     /* dlzka prave odosielaneho useku */
static volatile bool tx_busy = false;

static void rx_start(void)
{
    (void)HAL_UART_Receive_IT(port_uart, &rx_byte, 1U);
}

/* Spusti odoslanie suvisleho useku od tx_tail. Vola sa so zakazanymi preruseniami
 * alebo z obsluhy prerusenia. */
static void tx_start_next(void)
{
    const uint32_t used = tx_head - tx_tail;
    if (used == 0U)
    {
        tx_busy = false;
        return;
    }

    const uint32_t start = tx_tail & (SERIAL_PORT_TX_SIZE - 1U);
    uint32_t len = SERIAL_PORT_TX_SIZE - start;     /* po koniec pola */
    if (len > used)
    {
        len = used;
    }

    tx_chunk = (uint16_t)len;
    tx_busy = true;
    if (HAL_UART_Transmit_IT(port_uart, &tx_buf[start], (uint16_t)len) != HAL_OK)
    {
        tx_busy = false;                            /* skusi sa znova pri dalsom zapise */
    }
}

void SerialPort_Init(UART_HandleTypeDef *huart)
{
    port_uart = huart;
    rx_head = rx_tail = 0;
    tx_head = tx_tail = 0;
    tx_busy = false;
    rx_dropped = 0;
    rx_start();
}

bool SerialPort_ReadChar(char *c)
{
    if (rx_tail == rx_head)
    {
        return false;
    }
    *c = (char)rx_buf[rx_tail & (SERIAL_PORT_RX_SIZE - 1U)];
    rx_tail = rx_tail + 1U;
    return true;
}

size_t SerialPort_Write(const char *data, size_t len)
{
    size_t written = 0;

    if (port_uart == NULL)
    {
        return 0;
    }
    /* volne miesto: tx_tail moze medzitym iba narastat, takze odhad je bezpecny */
    while (written < len && (tx_head - tx_tail) < SERIAL_PORT_TX_SIZE)
    {
        tx_buf[tx_head & (SERIAL_PORT_TX_SIZE - 1U)] = (uint8_t)data[written++];
        tx_head = tx_head + 1U;
    }

    /* kratko zakazeme prerusenia, aby sa odosielanie nespustilo dvakrat
     * (hlavna slucka aj TxCpltCallback by naraz videli tx_busy == false) */
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (!tx_busy)
    {
        tx_start_next();
    }
    if (primask == 0U)
    {
        __enable_irq();
    }

    return written;
}

uint32_t SerialPort_RxDropped(void)
{
    return rx_dropped;
}

/* ---------------------------------------------------------------------------
 * Callbacky HAL - volaju sa z obsluhy prerusenia USART2_IRQHandler.
 * Musia byt kratke: iba presunu bajt a spustia dalsi prenos.
 * ------------------------------------------------------------------------- */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != port_uart)
    {
        return;
    }
    if ((rx_head - rx_tail) < SERIAL_PORT_RX_SIZE)
    {
        rx_buf[rx_head & (SERIAL_PORT_RX_SIZE - 1U)] = rx_byte;
        rx_head = rx_head + 1U;
    }
    else
    {
        rx_dropped = rx_dropped + 1U;               /* plny buffer, bajt sa zahodi */
    }
    rx_start();                                     /* hned cakame na dalsi bajt */
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != port_uart)
    {
        return;
    }
    tx_tail = tx_tail + tx_chunk;                   /* usek je odoslany */
    tx_start_next();                                /* pripadne zvysok (po pretoceni buffera) */
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != port_uart)
    {
        return;
    }
    /* Pri chybe (napr. pretecenie ORE, chyba ramca FE) HAL ukonci prijem.
     * Bez opatovneho spustenia by zariadenie prestalo prijimat prikazy. */
    rx_start();
}
