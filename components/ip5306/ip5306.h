#ifndef IP5306_H
#define IP5306_H

#include <stdbool.h>
#include "esp_err.h"

#define IP5306_I2C_ADDR 0x75

esp_err_t ip5306_init();
esp_err_t ip5306_get_battery_level(int *level);
esp_err_t ip5306_is_charging(bool *charging);
esp_err_t ip5306_is_full(bool *full);
esp_err_t ip5306_set_keep_awake(bool enable);

#endif // IP5306_H
