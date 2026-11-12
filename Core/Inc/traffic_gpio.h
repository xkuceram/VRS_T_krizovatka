#ifndef TRAFFIC_GPIO_H
#define TRAFFIC_GPIO_H

#include "traffic_phase.h"

void TrafficGpio_Init(void);
void TrafficGpio_Apply(TrafficPhase phase);
TrafficPhase TrafficGpio_GetPhase(void);

#endif
