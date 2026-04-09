#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "ip5306.h"
#include "sh1107.h"
#include "mpu6886.h"
#include "screens.h"

#define BUTTON_A_PIN      35                      // Button A is on GPIO 35 (Active Low)

static bool   blink = false;                 // used for the battery‑blink
static int    screen_mode = 4;                // cycle: 0=Ball/Z, 1=Z/Ball, 2=Ball/X(V), 3=X(V)/Z, 4=Kalman X/Z
static bool   last_btn_state = true;          // last button state for edge detection

void app_main(void)
{
    printf("M5Stick (Original) Initializing...\n");

    // Initialize Button A
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BUTTON_A_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    if (ip5306_init() != ESP_OK) {
        printf("Failed to init IP5306!\n");
    }
    if (mpu6886_init() != ESP_OK) {
        printf("Failed to init MPU6886!\n");
    }
    if (sh1107_init() != ESP_OK) {
        printf("Failed to init SH1107!\n");
    }

    screens_init();

    // Subscribe to TWDT
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 5000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&twdt_config);
    esp_task_wdt_add(NULL);

    while (1) {
        /* Check Button A (Active Low) */
        bool btn_state = gpio_get_level(BUTTON_A_PIN);
        if (last_btn_state && !btn_state) {
            screen_mode = (screen_mode + 1) % 5;
            printf("Screen Mode: %d\n", screen_mode);
        }
        last_btn_state = btn_state;

        /* 1. Read sensors */
        mpu6886_data_t accel, gyro;
        int batt_level = 0;
        bool charging = false;
        ip5306_get_battery_level(&batt_level);
        ip5306_is_charging(&charging);
        mpu6886_get_accel(&accel);
        mpu6886_get_gyro(&gyro);

        /* 2. Update and Draw */
        screens_update_data(&accel);
        screens_draw(screen_mode, &accel, batt_level, charging, blink);

        /* 3. Maintenance */
        esp_task_wdt_reset();
        blink = !blink;
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}
