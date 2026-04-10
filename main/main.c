#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "ip5306.h"
#include "sh1107.h"
#include "mpu6886.h"
#include "screens.h"
#include "common_state.h"
#include "sensor_processor.h"
#include "sensor_task.h"
#include "display_task.h"
#include "ble_task.h"
#include "alarm_task.h"

// Define global shared state
shared_state_t g_state = {
    .screen_mode = 5,
    .blink = false
};
SemaphoreHandle_t g_state_mutex = NULL;

void app_main(void)
{
    printf("M5Stick (Original) Multi-threaded (Split) Init...\n");

    // Initialize Mutex and Processors
    g_state_mutex = xSemaphoreCreateMutex();
    sensor_processor_init();

    // Initialize Hardware
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BUTTON_A_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    if (ip5306_init() != ESP_OK) printf("Failed to init IP5306!\n");
    if (mpu6886_init() != ESP_OK) printf("Failed to init MPU6886!\n");
    if (sh1107_init() != ESP_OK) printf("Failed to init SH1107!\n");

    screens_init();

    // Create Tasks
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 4, NULL);
    xTaskCreate(ble_task, "ble_task", 4096, NULL, 3, NULL);
    xTaskCreate(alarm_task, "alarm_task", 2048, NULL, 6, NULL); // Highest priority for safety

    // Main loop: Heartbeat
    uint32_t count = 0;
    while (1) {
        printf("Heartbeat %"PRIu32"...\n", count++);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
