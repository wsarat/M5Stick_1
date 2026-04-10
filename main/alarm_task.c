#include "alarm_task.h"
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "sensor_processor.h"
#include "common_state.h"

#define BUZZER_GPIO       26
#define LED_GPIO          19 // Blue LED (Active Low)
#define YAW_WARN_LIMIT    50.0f
#define YAW_CRIT_LIMIT    80.0f

static void buzzer_init(void)
{
    // LEDC timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .freq_hz          = 2000, // 2kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // LEDC channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = BUZZER_GPIO,
        .duty           = 0, // Silent initially
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    // LED configuration
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);
    gpio_set_level(LED_GPIO, 1); // OFF (Active Low)
}

static void buzzer_beep(int freq, int duty)
{
    if (freq > 0) {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    } else {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    }
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void alarm_task(void *pvParameters)
{
    buzzer_init();

    while (1) {
        sensor_processor_lock();
        processed_sensor_data_t *data = sensor_processor_get_data();
        float yaw_rate = fabsf(data->current_yaw_rate);
        sensor_processor_unlock();

        if (yaw_rate >= YAW_CRIT_LIMIT) {
            // CRITICAL: Rapid double beep, LED ON
            gpio_set_level(LED_GPIO, 0); // ON
            buzzer_beep(3000, 512);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_beep(0, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
            buzzer_beep(3000, 512);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_beep(0, 0);
            gpio_set_level(LED_GPIO, 1); // OFF
        } 
        else if (yaw_rate >= YAW_WARN_LIMIT) {
            // WARNING: Slow beep, LED flash
            gpio_set_level(LED_GPIO, 0); // ON
            buzzer_beep(1500, 256);
            vTaskDelay(pdMS_TO_TICKS(200));
            buzzer_beep(0, 0);
            gpio_set_level(LED_GPIO, 1); // OFF
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
