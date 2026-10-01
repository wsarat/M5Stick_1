#ifndef IP5306_H
#define IP5306_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define IP5306_I2C_ADDR 0x75

esp_err_t ip5306_init();
esp_err_t ip5306_get_battery_level(int *level);
esp_err_t ip5306_is_charging(bool *charging);
esp_err_t ip5306_is_full(bool *full);
esp_err_t ip5306_set_keep_awake(bool enable);

/**
 * @brief Log all known IP5306 registers (SYS 0x00-02, CHG 0x20-24, status 0x70-78).
 */
esp_err_t ip5306_dump_regs(void);

/**
 * @brief Read the power button (IP5306 KEY) register and report changes.
 *
 * Bit 0 of reg 0x77 latches high on this hardware and cannot be cleared
 * by writing, so the event flags are only set when the register value
 * CHANGES between two polls.
 *
 * @param raw    Optional: raw reg 0x77 value of this read.
 */
esp_err_t ip5306_get_key_events(uint8_t *raw, bool *short_press, bool *long_press, bool *double_press);

/**
 * @brief Estimate battery voltage (mV) from the 4-level gauge (NOT a measurement).
 * @param level    Battery percentage (0/25/50/75/100).
 * @param charging Whether the charger is active (different voltage bands).
 */
int ip5306_estimate_voltage_mv(int level, bool charging);

#endif // IP5306_H
