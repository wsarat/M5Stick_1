#pragma once

#include "esp_err.h"

#define MPU6886_I2C_ADDR 0x68

typedef struct {
    float x;
    float y;
    float z;
} mpu6886_data_t;

/**
 * @brief Initialize the MPU6886.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t mpu6886_init(void);

/**
 * @brief Get accelerometer data.
 * @param accel Pointer to mpu6886_data_t to store accelerometer data in g.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t mpu6886_get_accel(mpu6886_data_t *accel);

/**
 * @brief Get gyroscope data.
 * @param gyro Pointer to mpu6886_data_t to store gyroscope data in deg/s.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t mpu6886_get_gyro(mpu6886_data_t *gyro);
esp_err_t mpu6886_get_temp(double *temp);