#ifndef CPU_TEMP_H
#define CPU_TEMP_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Diagnostic info from the last cpu_temp_get() call.
 */
typedef struct {
    uint8_t raw;       // raw code of the last attempt
    int attempts;      // attempts made in this call (1..N)
    int valid_total;   // valid samples since boot
    int total;         // total samples since boot
} cpu_temp_dbg_t;

/**
 * @brief Prepare the ESP32 die temperature sensor (call once at boot).
 */
esp_err_t cpu_temp_init(void);

/**
 * @brief Read the ESP32 die (CPU) temperature.
 *
 * Makes a few quick ROM-helper attempts and validates the raw code.
 * The sensor is flaky on this chip, so callers should sample frequently
 * (~10Hz), keep the last good value on failure, and smooth the results
 * (e.g. median of the last few valid samples).
 *
 * @param out_celsius Result in degrees Celsius (untouched on failure).
 * @param dbg         Optional: diagnostics of this call.
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if no valid sample.
 */
esp_err_t cpu_temp_get(float *out_celsius, cpu_temp_dbg_t *dbg);

#endif // CPU_TEMP_H
