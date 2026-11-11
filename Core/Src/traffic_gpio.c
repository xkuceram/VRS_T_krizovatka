#include "traffic_gpio.h"
#include "main.h"

#define GPIOA_RED_PINS (LED_A_RED_Pin | LED_B_RED_Pin | LED_C_RED_Pin | LED_P_RED_Pin)
#define GPIOA_YELLOW_GREEN_PINS (LED_A_YELLOW_Pin | LED_A_GREEN_Pin | LED_C_YELLOW_Pin | LED_P_GREEN_Pin)
#define GPIOB_YELLOW_GREEN_PINS (LED_B_YELLOW_Pin | LED_B_GREEN_Pin | LED_C_GREEN_Pin)

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint16_t red_pin;
} PhaseOutput;

static const PhaseOutput phase_outputs[TRAFFIC_PHASE_COUNT] =
{
    [TRAFFIC_PHASE_A_GREEN] = {LED_A_GREEN_GPIO_Port, LED_A_GREEN_Pin, LED_A_RED_Pin},
    [TRAFFIC_PHASE_A_YELLOW] = {LED_A_YELLOW_GPIO_Port, LED_A_YELLOW_Pin, LED_A_RED_Pin},
    [TRAFFIC_PHASE_B_GREEN] = {LED_B_GREEN_GPIO_Port, LED_B_GREEN_Pin, LED_B_RED_Pin},
    [TRAFFIC_PHASE_B_YELLOW] = {LED_B_YELLOW_GPIO_Port, LED_B_YELLOW_Pin, LED_B_RED_Pin},
    [TRAFFIC_PHASE_C_GREEN] = {LED_C_GREEN_GPIO_Port, LED_C_GREEN_Pin, LED_C_RED_Pin},
    [TRAFFIC_PHASE_C_YELLOW] = {LED_C_YELLOW_GPIO_Port, LED_C_YELLOW_Pin, LED_C_RED_Pin},
    [TRAFFIC_PHASE_P_GREEN] = {LED_P_GREEN_GPIO_Port, LED_P_GREEN_Pin, LED_P_RED_Pin}
};

static TrafficPhase applied_phase = TRAFFIC_PHASE_COUNT;

void TrafficGpio_Init(void)
{
    applied_phase = TRAFFIC_PHASE_COUNT;
    TrafficGpio_Apply(TRAFFIC_PHASE_ALL_RED);
}

void TrafficGpio_Apply(TrafficPhase phase)
{
    if ((unsigned int)phase >= (unsigned int)TRAFFIC_PHASE_COUNT)
    {
        phase = TRAFFIC_PHASE_ALL_RED;
    }

    if (phase == applied_phase)
    {
        return;
    }

    const PhaseOutput *output = &phase_outputs[phase];
    const uint16_t red_pins_to_set = (uint16_t)(GPIOA_RED_PINS & ~output->red_pin);

    HAL_GPIO_WritePin(GPIOA, GPIOA_YELLOW_GREEN_PINS, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIOB_YELLOW_GREEN_PINS, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, red_pins_to_set, GPIO_PIN_SET);

    if (phase != TRAFFIC_PHASE_ALL_RED)
    {
        HAL_GPIO_WritePin(GPIOA, output->red_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(output->port, output->pin, GPIO_PIN_SET);
    }

    applied_phase = phase;
}

TrafficPhase TrafficGpio_GetPhase(void)
{
    return applied_phase;
}
