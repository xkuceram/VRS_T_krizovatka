#ifndef TRAFFIC_GPIO_H
#define TRAFFIC_GPIO_H

#include <stdbool.h>
#include "traffic_fsm.h"

typedef struct
{
    TrafficColor a;
    TrafficColor b_r;
    TrafficColor b_d;
    TrafficColor c;
    TrafficColor p;
    bool a_s_on;
    bool c_s_on;
} TrafficGpioOutputs;

void TrafficGpio_Init(void);

/* Povodne rozhranie pre automat, ktory posiela jednu fazu. */
void TrafficGpio_Apply(TrafficPhase phase);

/* Stav overeny automatom. Volat z hlavnej slucky pri zmene vystupov. */
/* NULL alebo neplatna farba nastavi spolocnu cervenu. */
void TrafficGpio_ApplyOutputs(const TrafficGpioOutputs *outputs);

/* Po TrafficGpio_ApplyOutputs vracia TRAFFIC_PHASE_COUNT az do dalsej fazy. */
TrafficPhase TrafficGpio_GetPhase(void);

#endif
