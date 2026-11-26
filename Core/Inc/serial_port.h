#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

/*
 * Hardverova vrstva seriovej linky (USART2 cez ST-LINK, 9600 8N1).
 *
 * Prijem aj odosielanie bezia cez prerusenia a dva kruhove buffery:
 *   - prerusenie po prijati bajtu ho ulozi do RX buffera a hned spusti prijem dalsieho,
 *   - hlavna slucka si bajty vybera cez SerialPort_ReadChar,
 *   - odpoved sa zapise do TX buffera (SerialPort_Write) a odosiela sa na pozadi,
 *     hlavna slucka na odoslanie necaka.
 *
 * Index, ktory meni prerusenie a cita hlavna slucka (alebo naopak), je "volatile".
 * Kazdy index zapisuje iba jedna strana, preto netreba zakazovat prerusenia
 * okrem kratkeho spustenia odosielania v SerialPort_Write.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "stm32f3xx_hal.h"

#define SERIAL_PORT_RX_SIZE  64U    /* prijimaci buffer [B], musi byt mocnina 2 */
#define SERIAL_PORT_TX_SIZE  256U   /* vysielaci buffer [B], musi byt mocnina 2 */

/* Spusti prijem cez prerusenie. Volat po MX_USART2_UART_Init a po TrafficSerial_Init. */
void SerialPort_Init(UART_HandleTypeDef *huart);

/* Vyberie jeden prijaty znak. Vrati false, ak buffer je prazdny. Volat z hlavnej slucky. */
bool SerialPort_ReadChar(char *c);

/* Zaradi data na odoslanie a vrati, kolko bajtov sa zmestilo. Neblokuje. */
size_t SerialPort_Write(const char *data, size_t len);

/* Pocet bajtov zahodenych pre plny RX buffer (na ladenie) */
uint32_t SerialPort_RxDropped(void);

#endif
