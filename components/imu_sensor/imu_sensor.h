#ifndef IMU_SENSOR_H
#define IMU_SENSOR_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Simple 3D vector for accelerometer and gyroscope data.
 */
typedef struct {
    float x;
    float y;
    float z;
} imu_data_t;

/**
 * @brief Advanced IMU Configuration
 * Allows easy adjustment of filters and compensation factors.
 */
typedef struct {
    float filter_alpha;      // Low-pass filter alpha (0.0 to 1.0, lower is smoother)
    float yaw_comp_x;        // G-sensitivity compensation for Lateral G
    float yaw_comp_z;        // G-sensitivity compensation for Longitudinal G
    float gyro_z_bias;       // Static gyro bias correction
    float gyro_y_bias;
    float gyro_x_bias;
} imu_config_t;

/**
 * @brief Processed IMU State
 */
typedef struct {
    imu_data_t accel;        // Raw accelerometer data in G
    imu_data_t gyro;         // Raw gyroscope data in deg/s
    float yaw_rate;          // Compensated and filtered yaw rate (deg/s)
} imu_state_t;

/**
 * @brief Initialize the IMU sensor.
 * @return ESP_OK on success.
 */
esp_err_t imu_sensor_init(void);

/**
 * @brief Set the processing configuration (filters, compensation).
 */
void imu_sensor_set_config(const imu_config_t *config);

/**
 * @brief Perform a sensor update. Reads hardware and updates internal filtered state.
 * @return ESP_OK on success.
 */
esp_err_t imu_sensor_update(void);

/**
 * @brief Get the latest processed IMU state.
 * @param state Pointer to imu_state_t to store the result.
 */
void imu_sensor_get_state(imu_state_t *state);

// Low-level raw access if needed
esp_err_t imu_sensor_get_accel_raw(imu_data_t *accel);
esp_err_t imu_sensor_get_gyro_raw(imu_data_t *gyro);

#endif // IMU_SENSOR_H
