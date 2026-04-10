#include "display_task.h"
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "screens.h"
#include "sensor_processor.h"
#include "common_state.h"

void display_task(void *pvParameters)
{
    while (1) {
        shared_state_t local_state;
        
        /* Copy shared state to local for drawing */
        if (xSemaphoreTake(g_state_mutex, portMAX_DELAY)) {
            g_state.blink = !g_state.blink;
            local_state = g_state;
            xSemaphoreGive(g_state_mutex);
        }

        /* Access processed data for drawing */
        sensor_processor_lock();
        processed_sensor_data_t *proc_data = sensor_processor_get_data();

        /* Draw */
        screens_draw(local_state.screen_mode, &local_state.imu, proc_data,
                     local_state.batt_level, local_state.charging, local_state.blink);
        
        sensor_processor_unlock();

        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz display refresh
    }
}
