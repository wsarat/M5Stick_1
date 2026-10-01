#include "imu_sensor.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IMU_SENSOR";

#define I2C_MASTER_NUM I2C_NUM_0
#define MPU9250_I2C_ADDR 0x68

// Registers (MPU9250 specific)
#define MPU_WHO_AM_I     0x75
#define MPU_PWR_MGMT_1   0x6B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_GYRO_CONFIG  0x1B
#define MPU_ACCEL_XOUT_H 0x3B
#define MPU_TEMP_OUT_H   0x41
#define MPU_GYRO_XOUT_H  0x43

// I2C handles
static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t i2c_dev_handle = NULL;

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

// Die temperature conversion: Temp_degC = TEMP_OUT / g_temp_sens + g_temp_offset
static float g_temp_sens = 333.87f; // MPU9250 datasheet value (LSB/degC)
static float g_temp_offset = 21.0f;

static esp_err_t imu_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_transmit(i2c_dev_handle, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

static esp_err_t imu_read_regs(uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(i2c_dev_handle, &reg, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

esp_err_t imu_sensor_init(void) {
    if (i2c_dev_handle) return ESP_OK;

    // Reuse the bus if another component already created it
    esp_err_t ret = i2c_master_get_bus_handle(I2C_MASTER_NUM, &i2c_bus_handle);
    if (ret != ESP_OK) {
        i2c_master_bus_config_t i2c_mst_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = I2C_MASTER_NUM,
            .scl_io_num = 22,   // SCL from M5Stick.md
            .sda_io_num = 21,   // SDA from M5Stick.md
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        ret = i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    // Add I2C device
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU9250_I2C_ADDR,
        .scl_speed_hz = 400000,  // 400kHz
    };
    
    ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        i2c_bus_handle = NULL;
        return ret;
    }

    uint8_t who_am_i;
    esp_err_t err = imu_read_regs(MPU_WHO_AM_I, &who_am_i, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I");
        i2c_master_bus_rm_device(i2c_dev_handle);
        i2c_dev_handle = NULL;
        i2c_bus_handle = NULL;
        return err;
    }

    ESP_LOGI(TAG, "IMU WHO_AM_I: 0x%02x", who_am_i);

    // Check if it's MPU9250 (0x71) or MPU6886 (0x38) - accept either for compatibility
    if (who_am_i != 0x71 && who_am_i != 0x38) {
        ESP_LOGE(TAG, "Unexpected WHO_AM_I value: 0x%02x", who_am_i);
        i2c_master_bus_rm_device(i2c_dev_handle);
        i2c_dev_handle = NULL;
        i2c_bus_handle = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    // Temperature conversion constants (Temp_degC = TEMP_OUT / sens + offset)
    if (who_am_i == 0x38) { // MPU6886
        g_temp_sens = 326.8f;
        g_temp_offset = 25.0f;
    } else { // MPU9250
        g_temp_sens = 333.87f;
        g_temp_offset = 21.0f;
    }

    // Initial wake-up and basic config for MPU9250
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
    // Single burst: accel (0x3B-0x40), temperature (0x41-0x42), gyro (0x43-0x4C)
    uint8_t data[14];
    esp_err_t err = imu_read_regs(MPU_ACCEL_XOUT_H, data, sizeof(data));
    if (err != ESP_OK) return err;

    int16_t ax = (int16_t)((data[0] << 8) | data[1]);
    int16_t ay = (int16_t)((data[2] << 8) | data[3]);
    int16_t az = (int16_t)((data[4] << 8) | data[5]);
    int16_t temp_raw = (int16_t)((data[6] << 8) | data[7]);
    int16_t gx = (int16_t)((data[8] << 8) | data[9]);
    int16_t gy = (int16_t)((data[10] << 8) | data[11]);
    int16_t gz = (int16_t)((data[12] << 8) | data[13]);

    imu_data_t raw_accel = {
        .x = (float)ax * accel_res,
        .y = (float)ay * accel_res,
        .z = (float)az * accel_res,
    };
    imu_data_t raw_gyro = {
        .x = (float)gx * gyro_res,
        .y = (float)gy * gyro_res,
        .z = (float)gz * gyro_res,
    };

    // Store raw data
    g_imu_state.accel = raw_accel;
    g_imu_state.gyro = raw_gyro;
    g_imu_state.temp_raw = temp_raw;
    g_imu_state.temp_c = ((float)temp_raw / g_temp_sens) + g_temp_offset;

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
