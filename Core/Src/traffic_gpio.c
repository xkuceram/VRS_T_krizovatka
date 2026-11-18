#include "traffic_gpio.h"
#include "main.h"
#include <stddef.h>

enum
{
    RGB_A,
    RGB_B_R,
    RGB_B_D,
    RGB_C,
    RGB_P,
    RGB_COUNT
};

typedef struct
{
    GPIO_TypeDef *red_port;
    uint16_t red_pin;
    GPIO_TypeDef *green_port;
    uint16_t green_pin;
} RgbPins;

static const RgbPins rgb_pins[RGB_COUNT] =
{
    [RGB_A]   = {LED_A_RED_GPIO_Port, LED_A_RED_Pin, LED_A_GREEN_GPIO_Port, LED_A_GREEN_Pin},
    [RGB_B_R] = {LED_BR_RED_GPIO_Port, LED_BR_RED_Pin, LED_BR_GREEN_GPIO_Port, LED_BR_GREEN_Pin},
    [RGB_B_D] = {LED_BD_RED_GPIO_Port, LED_BD_RED_Pin, LED_BD_GREEN_GPIO_Port, LED_BD_GREEN_Pin},
    [RGB_C]   = {LED_C_RED_GPIO_Port, LED_C_RED_Pin, LED_C_GREEN_GPIO_Port, LED_C_GREEN_Pin},
    [RGB_P]   = {LED_P_RED_GPIO_Port, LED_P_RED_Pin, LED_P_GREEN_GPIO_Port, LED_P_GREEN_Pin}
};

/* V povodnych fazach je B spolocny pre oba semafory zprava.
 * Predvolena hodnota je cervena (TRAFFIC_COLOR_RED = 0) alebo zhasnuta sipka.
 */
static const TrafficGpioOutputs phase_outputs[TRAFFIC_PHASE_COUNT] =
{
    [TRAFFIC_PHASE_ALL_RED]  = {0},
    [TRAFFIC_PHASE_A_GREEN]  = {.a = TRAFFIC_COLOR_GREEN},
    [TRAFFIC_PHASE_A_YELLOW] = {.a = TRAFFIC_COLOR_YELLOW},
    [TRAFFIC_PHASE_B_GREEN]  = {.b_r = TRAFFIC_COLOR_GREEN, .b_d = TRAFFIC_COLOR_GREEN},
    [TRAFFIC_PHASE_B_YELLOW] = {.b_r = TRAFFIC_COLOR_YELLOW, .b_d = TRAFFIC_COLOR_YELLOW},
    [TRAFFIC_PHASE_C_GREEN]  = {.c = TRAFFIC_COLOR_GREEN},
    [TRAFFIC_PHASE_C_YELLOW] = {.c = TRAFFIC_COLOR_YELLOW},
    [TRAFFIC_PHASE_P_GREEN]  = {.p = TRAFFIC_COLOR_GREEN}
};

static TrafficPhase applied_phase = TRAFFIC_PHASE_COUNT;

static void write_outputs(const TrafficGpioOutputs *outputs)
{
    const TrafficColor colors[RGB_COUNT] =
    {
        [RGB_A] = outputs->a,
        [RGB_B_R] = outputs->b_r,
        [RGB_B_D] = outputs->b_d,
        [RGB_C] = outputs->c,
        [RGB_P] = outputs->p
    };
    bool valid_colors = true;

    /* Najprv zhasnut povodne zelene kanaly a sipky na oboch portoch. */
    HAL_GPIO_WritePin(LED_AS_GREEN_GPIO_Port, LED_AS_GREEN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_CS_GREEN_GPIO_Port, LED_CS_GREEN_Pin, GPIO_PIN_RESET);
    for (size_t i = 0; i < RGB_COUNT; i++)
    {
        HAL_GPIO_WritePin(rgb_pins[i].green_port, rgb_pins[i].green_pin, GPIO_PIN_RESET);
        if ((unsigned int)colors[i] > (unsigned int)TRAFFIC_COLOR_GREEN ||
            (i == RGB_P && colors[i] == TRAFFIC_COLOR_YELLOW))
        {
            valid_colors = false;
        }
    }

    /* R svieti pri cervenej aj zltej. Pri neplatnej farbe svietia vsetky R. */
    for (size_t i = 0; i < RGB_COUNT; i++)
    {
        GPIO_PinState red = valid_colors && colors[i] == TRAFFIC_COLOR_GREEN
            ? GPIO_PIN_RESET : GPIO_PIN_SET;
        HAL_GPIO_WritePin(rgb_pins[i].red_port, rgb_pins[i].red_pin, red);
    }

    if (!valid_colors)
    {
        return;
    }

    /* G svieti pri zelenej aj zltej. Pri zltej uz svieti aj R. */
    for (size_t i = 0; i < RGB_COUNT; i++)
    {
        if (colors[i] == TRAFFIC_COLOR_GREEN || colors[i] == TRAFFIC_COLOR_YELLOW)
        {
            HAL_GPIO_WritePin(rgb_pins[i].green_port, rgb_pins[i].green_pin, GPIO_PIN_SET);
        }
    }

    if (outputs->a_s_on)
    {
        HAL_GPIO_WritePin(LED_AS_GREEN_GPIO_Port, LED_AS_GREEN_Pin, GPIO_PIN_SET);
    }
    if (outputs->c_s_on)
    {
        HAL_GPIO_WritePin(LED_CS_GREEN_GPIO_Port, LED_CS_GREEN_Pin, GPIO_PIN_SET);
    }
}

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

    write_outputs(&phase_outputs[phase]);
    applied_phase = phase;
}

void TrafficGpio_ApplyOutputs(const TrafficGpioOutputs *outputs)
{
    if (outputs == NULL)
    {
        outputs = &phase_outputs[TRAFFIC_PHASE_ALL_RED];
    }

    write_outputs(outputs);
    applied_phase = TRAFFIC_PHASE_COUNT;
}

TrafficPhase TrafficGpio_GetPhase(void)
{
    return applied_phase;
}
