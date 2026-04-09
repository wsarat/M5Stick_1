#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "ip5306.h"
#include "sh1107.h"
#include "mpu6886.h"
#include "screens.h"

#define BUTTON_A_PIN      35

// Shared state structure
typedef struct {
    mpu6886_data_t accel;
    int batt_level;
    bool charging;
    int screen_mode;
    bool blink;
} shared_state_t;

static shared_state_t g_state = {
    .screen_mode = 4,
    .blink = false
};
static SemaphoreHandle_t g_state_mutex = NULL;

/**
 * @brief Task 1: Sensor Reading
 * Periodically reads MPU6886, IP5306, and Button A.
 */
static void sensor_task(void *pvParameters)
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
        mpu6886_data_t accel;
        int batt_level = 0;
        bool charging = false;
        
        mpu6886_get_accel(&accel);
        ip5306_get_battery_level(&batt_level);
        ip5306_is_charging(&charging);

        // Update screens internal graph buffers (thread-safe internally)
        screens_update_data(&accel);

        /* Update shared state */
        if (xSemaphoreTake(g_state_mutex, portMAX_DELAY)) {
            g_state.accel = accel;
            g_state.batt_level = batt_level;
            g_state.charging = charging;
            if (new_mode == 0) {
                g_state.screen_mode = (g_state.screen_mode + 1) % 5;
                printf("Screen Mode: %d\n", g_state.screen_mode);
            }
            xSemaphoreGive(g_state_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz sensing
    }
}

/**
 * @brief Task 2: Screen Update
 * Periodically draws to the SH1107 OLED.
 */
static void display_task(void *pvParameters)
{
    while (1) {
        shared_state_t local_state;
        
        /* Copy shared state to local for drawing */
        if (xSemaphoreTake(g_state_mutex, portMAX_DELAY)) {
            g_state.blink = !g_state.blink;
            local_state = g_state;
            xSemaphoreGive(g_state_mutex);
        }

        /* Draw (thread-safe internally for buffers) */
        screens_draw(local_state.screen_mode, &local_state.accel, 
                     local_state.batt_level, local_state.charging, local_state.blink);

        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz display refresh
    }
}

void app_main(void)
{
    printf("M5Stick (Original) Multi-threaded Init...\n");

    // Initialize Mutex
    g_state_mutex = xSemaphoreCreateMutex();

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

    // Main loop: Heartbeat
    uint32_t count = 0;
    while (1) {
        printf("Heartbeat %"PRIu32"...\n", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
