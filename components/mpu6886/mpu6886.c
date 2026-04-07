#include "mpu6886.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MPU6886";

#define I2C_MASTER_NUM I2C_NUM_0

// Registers
#define MPU6886_WHO_AM_I     0x75
#define MPU6886_PWR_MGMT_1   0x6B
#define MPU6886_ACCEL_CONFIG 0x1C
#define MPU6886_GYRO_CONFIG  0x1B
#define MPU6886_ACCEL_XOUT_H 0x3B
#define MPU6886_GYRO_XOUT_H  0x43

// Sensitivity
static const float accel_res = 8.0 / 32768.0; // +/- 8g
static const float gyro_res = 2000.0 / 32768.0; // +/- 2000 deg/s

static esp_err_t mpu6886_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MPU6886_I2C_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

static esp_err_t mpu6886_read_regs(uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, MPU6886_I2C_ADDR, &reg, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

esp_err_t mpu6886_init(void) {
    uint8_t who_am_i;
    esp_err_t err = mpu6886_read_regs(MPU6886_WHO_AM_I, &who_am_i, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I");
        return err;
    }
    ESP_LOGI(TAG, "MPU6886 WHO_AM_I: 0x%02x", who_am_i);

    // Wake up
    mpu6886_write_reg(MPU6886_PWR_MGMT_1, 0x00);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    // Set Accel Range to +/- 8g (0x10)
    mpu6886_write_reg(MPU6886_ACCEL_CONFIG, 0x10);
    
    // Set Gyro Range to +/- 2000 deg/s (0x18)
    mpu6886_write_reg(MPU6886_GYRO_CONFIG, 0x18);
    
    return ESP_OK;
}

esp_err_t mpu6886_get_accel(mpu6886_data_t *accel) {
    uint8_t data[6];
    esp_err_t err = mpu6886_read_regs(MPU6886_ACCEL_XOUT_H, data, 6);
    if (err != ESP_OK) return err;
    
    int16_t ax = (int16_t)((data[0] << 8) | data[1]);
    int16_t ay = (int16_t)((data[2] << 8) | data[3]);
    int16_t az = (int16_t)((data[4] << 8) | data[5]);
    
    accel->x = (float)ax * accel_res;
    accel->y = (float)ay * accel_res;
    accel->z = (float)az * accel_res;
    
    return ESP_OK;
}

esp_err_t mpu6886_get_gyro(mpu6886_data_t *gyro) {
    uint8_t data[6];
    esp_err_t err = mpu6886_read_regs(MPU6886_GYRO_XOUT_H, data, 6);
    if (err != ESP_OK) return err;
    
    int16_t gx = (int16_t)((data[0] << 8) | data[1]);
    int16_t gy = (int16_t)((data[2] << 8) | data[3]);
    int16_t gz = (int16_t)((data[4] << 8) | data[5]);
    
    gyro->x = (float)gx * gyro_res;
    gyro->y = (float)gy * gyro_res;
    gyro->z = (float)gz * gyro_res;
    
    return ESP_OK;
}

esp_err_t mpu6886_get_temp(double *temp) {
    uint8_t data[2];
    esp_err_t err = mpu6886_read_regs(0x41, data, 2);
    if (err != ESP_OK) return err;
    
    *temp = ((double)((int16_t)(data[0] << 8 | data[1]))) / 340.0 + 21;
    
    return ESP_OK;
}

