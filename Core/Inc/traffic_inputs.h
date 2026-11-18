#ifndef TRAFFIC_INPUTS_H
#define TRAFFIC_INPUTS_H

#include <stdbool.h>
#include <stdint.h>

/* Inicializovat po MX_GPIO_Init. Aktualizovat v hlavnej slucke s HAL_GetTick(). */
void TrafficInputs_Init(uint32_t now_ms);
void TrafficInputs_Update(uint32_t now_ms);

/* Cakajuce stlacenie. Precitanim sa priznak vynuluje. */
bool TrafficInputs_TakePedestrianPress(void);

/* Vyfiltrovany stav obsadenosti vjazdu C. */
bool TrafficInputs_IsCarPresent(void);

#endif
