#ifndef COMMON_STATE_H
#define COMMON_STATE_H

#include "imu_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Shared state structure
typedef struct {
    imu_state_t imu;
    int batt_level;
    bool charging;
    int screen_mode;
    bool blink;
    float cpu_temp_c;
} shared_state_t;

extern shared_state_t g_state;
extern SemaphoreHandle_t g_state_mutex;

#define BUTTON_A_PIN      35

#endif // COMMON_STATE_H
