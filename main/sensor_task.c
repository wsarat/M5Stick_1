#include "sensor_task.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "imu_sensor.h"
#include "ip5306.h"
#include "sensor_processor.h"
#include "common_state.h"
#include "cpu_temp.h"
#include "esp_log.h"

static const char *TAG = "TEMP_DBG";

#define CPU_TEMP_SAMPLE_TICKS 5    // sample every 100ms
#define CPU_TEMP_LOG_SAMPLES  20   // log every 2s
#define CPU_TEMP_RING_LEN     5

/* Median of the CPU raw ring: kills isolated spurious readings. */
static uint8_t cpu_raw_median(const uint8_t *ring, int n)
{
    uint8_t sorted[CPU_TEMP_RING_LEN];
    for (int i = 0; i < n; i++) sorted[i] = ring[i];
    for (int i = 1; i < n; i++) {
        uint8_t v = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > v) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = v;
    }
    return sorted[n / 2];
}

void sensor_task(void *pvParameters)
{
    bool last_btn_state = true;
    int cpu_temp_divider = 0;
    int cpu_log_divider = 0;
    float cpu_temp_c = NAN;
    cpu_temp_dbg_t cpu_dbg = {0};
    bool cpu_valid = false;
    bool cpu_log_pending = false;
    uint8_t cpu_ring[CPU_TEMP_RING_LEN];
    int cpu_ring_n = 0;
    TickType_t key_log_tick = 0;
    while (1) {
        /* Check Button A (Active Low) */
        bool btn_state = gpio_get_level(BUTTON_A_PIN);
        int new_mode = -1;
        if (last_btn_state && !btn_state) {
            new_mode = 0; // Trigger increment
        }
        last_btn_state = btn_state;

        /* CPU die temperature: sample every 100ms, publish median of the
         * last valid raws (the sensor glitches, see cpu_temp.c), log every 2s */
        if (++cpu_temp_divider >= CPU_TEMP_SAMPLE_TICKS) {
            cpu_temp_divider = 0;
            float sample;
            cpu_valid = (cpu_temp_get(&sample, &cpu_dbg) == ESP_OK);
            if (cpu_valid) {
                if (cpu_ring_n < CPU_TEMP_RING_LEN) {
                    cpu_ring[cpu_ring_n++] = cpu_dbg.raw;
                } else {
                    memmove(cpu_ring, cpu_ring + 1, CPU_TEMP_RING_LEN - 1);
                    cpu_ring[CPU_TEMP_RING_LEN - 1] = cpu_dbg.raw;
                }
                cpu_temp_c = ((float)cpu_raw_median(cpu_ring, cpu_ring_n) - 32.0f) / 1.8f;
            }
            if (++cpu_log_divider >= CPU_TEMP_LOG_SAMPLES) {
                cpu_log_divider = 0;
                cpu_log_pending = true;
            }
        }

        /* Read hardware and process (filtering, G-comp) inside imu_sensor component */
        if (imu_sensor_update() == ESP_OK) {
            imu_state_t current_imu;
            imu_sensor_get_state(&current_imu);

            int batt_level = 0;
            bool charging = false;
            bool full = false;
            ip5306_get_battery_level(&batt_level);
            ip5306_is_charging(&charging);
            ip5306_is_full(&full);

            uint8_t key_raw = 0;
            bool key_short = false, key_long = false, key_double = false;
            if (ip5306_get_key_events(&key_raw, &key_short, &key_long, &key_double) == ESP_OK &&
                (key_short || key_long || key_double)) {
                TickType_t now = xTaskGetTickCount();
                if (now - key_log_tick >= pdMS_TO_TICKS(500)) {
                    key_log_tick = now;
                    ESP_LOGI(TAG, "IP5306 KEY raw=0x%02X: %s%s%s", key_raw,
                             key_short ? "short " : "", key_long ? "long " : "",
                             key_double ? "double" : "");
                }
            }

            // Update UI graph processor
            sensor_processor_update(&current_imu);

            if (cpu_log_pending) {
                cpu_log_pending = false;
                int batt_mv = ip5306_estimate_voltage_mv(batt_level, charging);
                ESP_LOGI(TAG, "CPU raw=%u %s -> %.1fC (valid %d/%d) | IMU raw=%d -> %.2fC | "
                              "BATT %d%% ~%dmV chg=%d full=%d",
                         cpu_dbg.raw,
                         cpu_valid ? "ok" : "bad, keeping last",
                         cpu_temp_c,
                         cpu_dbg.valid_total, cpu_dbg.total,
                         current_imu.temp_raw, current_imu.temp_c,
                         batt_level, batt_mv, charging ? 1 : 0, full ? 1 : 0);
            }

            /* Update shared state */
            if (xSemaphoreTake(g_state_mutex, portMAX_DELAY)) {
                g_state.imu = current_imu;
                g_state.batt_level = batt_level;
                g_state.charging = charging;
                g_state.cpu_temp_c = cpu_temp_c;
                if (new_mode == 0) {
                    g_state.screen_mode = (g_state.screen_mode + 1) % 8;
                    printf("Screen Mode: %d\n", g_state.screen_mode);
                }
                xSemaphoreGive(g_state_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz sensing
    }
}
