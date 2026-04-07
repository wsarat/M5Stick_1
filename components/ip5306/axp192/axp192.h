#ifndef AXP192_H
#define AXP192_H

#include "esp_err.h"

#define AXP192_I2C_ADDR 0x34

esp_err_t axp192_init();
esp_err_t axp192_read_vbat(float *vbat);
esp_err_t axp192_read_vaps(float *vaps);
esp_err_t axp192_read_vbus(float *vbus);
esp_err_t axp192_read_icharge(float *icharge);
esp_err_t axp192_read_idischarge(float *idischarge);
esp_err_t axp192_read_temp(float *temp);

#endif // AXP192_H
