#include "imu_sensor.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IMU_SENSOR";

#define I2C_MASTER_NUM I2C_NUM_0
#define MPU6886_I2C_ADDR 0x68

// Registers (MPU6886 specific)
#define MPU_WHO_AM_I     0x75
#define MPU_PWR_MGMT_1   0x6B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_GYRO_CONFIG  0x1B
#define MPU_ACCEL_XOUT_H 0x3B
#define MPU_GYRO_XOUT_H  0x43

// Sensitivity
static const float accel_res = 8.0 / 32768.0; // +/- 8g
static const float gyro_res = 2000.0 / 32768.0; // +/- 2000 deg/s

// Internal State
static imu_config_t g_imu_config = {
    .filter_alpha = 0.15f,
    .yaw_comp_x = 1.8f,
    .yaw_comp_z = 0.5f,
    .gyro_y_bias = 0.0f
};

static imu_state_t g_imu_state = {0};

static esp_err_t imu_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MPU6886_I2C_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

static esp_err_t imu_read_regs(uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, MPU6886_I2C_ADDR, &reg, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

esp_err_t imu_sensor_init(void) {
    uint8_t who_am_i;
    esp_err_t err = imu_read_regs(MPU_WHO_AM_I, &who_am_i, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I");
        return err;
    }
    ESP_LOGI(TAG, "IMU WHO_AM_I: 0x%02x", who_am_i);

    // Initial wake-up and basic config for MPU6886
    imu_write_reg(MPU_PWR_MGMT_1, 0x00);
    vTaskDelay(pdMS_TO_TICKS(10));
    imu_write_reg(MPU_ACCEL_CONFIG, 0x10); // +/- 8g
    imu_write_reg(MPU_GYRO_CONFIG, 0x18);  // +/- 2000 deg/s
    
    return ESP_OK;
}

void imu_sensor_set_config(const imu_config_t *config) {
    if (config) {
        g_imu_config = *config;
    }
}

esp_err_t imu_sensor_get_accel_raw(imu_data_t *accel) {
    uint8_t data[6];
    esp_err_t err = imu_read_regs(MPU_ACCEL_XOUT_H, data, 6);
    if (err != ESP_OK) return err;
    
    int16_t ax = (int16_t)((data[0] << 8) | data[1]);
    int16_t ay = (int16_t)((data[2] << 8) | data[3]);
    int16_t az = (int16_t)((data[4] << 8) | data[5]);
    
    accel->x = (float)ax * accel_res;
    accel->y = (float)ay * accel_res;
    accel->z = (float)az * accel_res;
    
    return ESP_OK;
}

esp_err_t imu_sensor_get_gyro_raw(imu_data_t *gyro) {
    uint8_t data[6];
    esp_err_t err = imu_read_regs(MPU_GYRO_XOUT_H, data, 6);
    if (err != ESP_OK) return err;
    
    int16_t gx = (int16_t)((data[0] << 8) | data[1]);
    int16_t gy = (int16_t)((data[2] << 8) | data[3]);
    int16_t gz = (int16_t)((data[4] << 8) | data[5]);
    
    gyro->x = (float)gx * gyro_res;
    gyro->y = (float)gy * gyro_res;
    gyro->z = (float)gz * gyro_res;
    
    return ESP_OK;
}

esp_err_t imu_sensor_update(void) {
    imu_data_t raw_accel, raw_gyro;
    
    esp_err_t err = imu_sensor_get_accel_raw(&raw_accel);
    if (err != ESP_OK) return err;
    
    err = imu_sensor_get_gyro_raw(&raw_gyro);
    if (err != ESP_OK) return err;
    
    // Store raw data
    g_imu_state.accel = raw_accel;
    g_imu_state.gyro = raw_gyro;

    // Advanced Processing (G-Sensitivity Compensation)
    float raw_y_rate = raw_gyro.y - g_imu_config.gyro_y_bias;
    float comp_y_rate = raw_y_rate - (raw_accel.x * g_imu_config.yaw_comp_x) - (raw_accel.z * g_imu_config.yaw_comp_z);

    // Low-pass Filter
    g_imu_state.yaw_rate = (g_imu_state.yaw_rate * (1.0f - g_imu_config.filter_alpha)) + (comp_y_rate * g_imu_config.filter_alpha);

    // Deadzone
    if (fabsf(g_imu_state.yaw_rate) < 0.5f) {
        g_imu_state.yaw_rate = 0.0f;
    }

    return ESP_OK;
}

void imu_sensor_get_state(imu_state_t *state) {
    if (state) {
        *state = g_imu_state;
    }
}
