#ifndef COMMON_STATE_H
#define COMMON_STATE_H

#include "mpu6886.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Shared state structure
typedef struct {
    mpu6886_data_t accel;
    mpu6886_data_t gyro;
    int batt_level;
    bool charging;
    int screen_mode;
    bool blink;
} shared_state_t;

extern shared_state_t g_state;
extern SemaphoreHandle_t g_state_mutex;

#define BUTTON_A_PIN      35

#endif // COMMON_STATE_H
