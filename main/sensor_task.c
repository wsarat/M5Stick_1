#include "sensor_task.h"
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "mpu6886.h"
#include "ip5306.h"
#include "sensor_processor.h"
#include "common_state.h"

void sensor_task(void *pvParameters)
{
    bool last_btn_state = true;
    while (1) {
        /* Check Button A (Active Low) */
        bool btn_state = gpio_get_level(BUTTON_A_PIN);
        int new_mode = -1;
        if (last_btn_state && !btn_state) {
            new_mode = 0; // Trigger increment
        }
        last_btn_state = btn_state;

        /* Read sensors */
        mpu6886_data_t accel, gyro;
        int batt_level = 0;
        bool charging = false;
        
        mpu6886_get_accel(&accel);
        mpu6886_get_gyro(&gyro);
        ip5306_get_battery_level(&batt_level);
        ip5306_is_charging(&charging);

        // Update sensor processor (Kalman, graphs, yaw)
        sensor_processor_update(&accel, &gyro);

        /* Update shared state */
        if (xSemaphoreTake(g_state_mutex, portMAX_DELAY)) {
            g_state.accel = accel;
            g_state.gyro = gyro;
            g_state.batt_level = batt_level;
            g_state.charging = charging;
            if (new_mode == 0) {
                g_state.screen_mode = (g_state.screen_mode + 1) % 7;
                printf("Screen Mode: %d\n", g_state.screen_mode);
            }
            xSemaphoreGive(g_state_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz sensing
    }
}
