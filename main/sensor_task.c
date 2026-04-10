#include "sensor_task.h"
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "imu_sensor.h"
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

        /* Read hardware and process (filtering, G-comp) inside imu_sensor component */
        if (imu_sensor_update() == ESP_OK) {
            imu_state_t current_imu;
            imu_sensor_get_state(&current_imu);

            int batt_level = 0;
            bool charging = false;
            ip5306_get_battery_level(&batt_level);
            ip5306_is_charging(&charging);

            // Update UI graph processor
            sensor_processor_update(&current_imu);

            /* Update shared state */
            if (xSemaphoreTake(g_state_mutex, portMAX_DELAY)) {
                g_state.imu = current_imu;
                g_state.batt_level = batt_level;
                g_state.charging = charging;
                if (new_mode == 0) {
                    g_state.screen_mode = (g_state.screen_mode + 1) % 7;
                    printf("Screen Mode: %d\n", g_state.screen_mode);
                }
                xSemaphoreGive(g_state_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz sensing
    }
}
