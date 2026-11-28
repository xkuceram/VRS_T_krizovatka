#ifndef TRAFFIC_SERIAL_LINK_H
#define TRAFFIC_SERIAL_LINK_H

/*
 * Prepojenie protokolu USART (traffic_serial) so stavovym automatom (traffic_fsm).
 *
 * Protokol pozna iba svoje typy (SerialMode, SerialTarget, ...), automat svoje
 * (TrafficMode, TrafficTarget, ...). Tento modul ich navzajom prelozi a ponukne
 * protokolu tri funkcie automatu v strukture TrafficSerialHandlers.
 *
 * Modul nepouziva HAL, preto sa da otestovat na PC spolu so skutocnym automatom
 * (tests/test_serial_fsm.c).
 *
 * Pouzitie (po TrafficFsm_Init):
 *   TrafficSerial_Init(TrafficSerialLink_Handlers());
 */

#include "traffic_serial.h"

/* Funkcie automatu pre protokol (staticka struktura, plati pocas celeho behu) */
const TrafficSerialHandlers *TrafficSerialLink_Handlers(void);

#endif
