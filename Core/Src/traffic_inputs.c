#include "traffic_inputs.h"
#include "main.h"

#define BUTTON_DEBOUNCE_MS      30U
#define CAR_SENSOR_FILTER_MS    50U
#define CAR_SENSOR_ACTIVE_LEVEL GPIO_PIN_RESET

typedef struct
{
    bool candidate;
    bool stable;
    uint32_t candidate_since_ms;
} DebouncedInput;

static DebouncedInput pedestrian_button;
static DebouncedInput car_sensor;
static bool pedestrian_press_pending;

static bool read_pedestrian_button(void)
{
    return HAL_GPIO_ReadPin(P_BTN_GPIO_Port, P_BTN_Pin) == GPIO_PIN_RESET;
}

static void update_input(DebouncedInput *input, bool active,
                         uint32_t now_ms, uint32_t filter_ms)
{
    if (active != input->candidate)
    {
        input->candidate = active;
        input->candidate_since_ms = now_ms;
    }

    if (input->candidate != input->stable &&
        (uint32_t)(now_ms - input->candidate_since_ms) >= filter_ms)
    {
        input->stable = input->candidate;
    }
}

void TrafficInputs_Init(uint32_t now_ms)
{
    const bool pressed = read_pedestrian_button();

    pedestrian_button = (DebouncedInput){
        .candidate = pressed,
        .stable = pressed,
        .candidate_since_ms = now_ms
    };
    car_sensor = (DebouncedInput){.candidate_since_ms = now_ms};
    pedestrian_press_pending = false;
}

void TrafficInputs_Update(uint32_t now_ms)
{
    const bool was_pressed = pedestrian_button.stable;
    update_input(&pedestrian_button, read_pedestrian_button(), now_ms, BUTTON_DEBOUNCE_MS);
    if (!was_pressed && pedestrian_button.stable)
    {
        pedestrian_press_pending = true;
    }

    const bool car_detected = HAL_GPIO_ReadPin(CAR_C_GPIO_Port, CAR_C_Pin) == CAR_SENSOR_ACTIVE_LEVEL;
    update_input(&car_sensor, car_detected, now_ms, CAR_SENSOR_FILTER_MS);
}

bool TrafficInputs_TakePedestrianPress(void)
{
    const bool pressed = pedestrian_press_pending;
    pedestrian_press_pending = false;
    return pressed;
}

bool TrafficInputs_IsCarPresent(void)
{
    return car_sensor.stable;
}
