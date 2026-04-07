#include "axp192.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "AXP192";

#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000 // Lower frequency for stability
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

static esp_err_t i2c_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) return err;
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

void i2c_scan() {
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (int i = 1; i < 127; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t espRc = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 10 / portTICK_PERIOD_MS);
        if (espRc == ESP_OK) {
            ESP_LOGI(TAG, "Found device at address: 0x%02x", i);
        }
        i2c_cmd_link_delete(cmd);
    }
}

static esp_err_t axp192_read_reg(uint8_t reg, uint8_t *data, size_t len) {
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, AXP192_I2C_ADDR, &reg, 1, data, len, 1000 / portTICK_PERIOD_MS);
    return err;
}

static esp_err_t axp192_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM, AXP192_I2C_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
    return err;
}

esp_err_t axp192_init() {
    esp_err_t err = i2c_init();
    if (err != ESP_OK) return err;

    i2c_scan();

    // Probe device
    uint8_t test_val;
    err = axp192_read_reg(0x03, &test_val, 1); 
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AXP192 not responding at 0x%02X", AXP192_I2C_ADDR);
        return err;
    }
    ESP_LOGI(TAG, "AXP192 detected! ID: 0x%02X", test_val);

    axp192_write_reg(0x12, 0x4D); 
    axp192_write_reg(0x82, 0xFF); 

    return ESP_OK;
}

esp_err_t axp192_read_vbat(float *vbat) {
    uint8_t buf[2];
    esp_err_t err = axp192_read_reg(0x78, buf, 2);
    if (err != ESP_OK) return err;
    uint16_t res = (buf[0] << 4) | (buf[1] & 0x0F);
    *vbat = res * 1.1 / 1000.0;
    return ESP_OK;
}

esp_err_t axp192_read_vaps(float *vaps) {
    uint8_t buf[2];
    esp_err_t err = axp192_read_reg(0x7F, buf, 2);
    if (err != ESP_OK) return err;
    uint16_t res = (buf[0] << 4) | (buf[1] & 0x0F);
    *vaps = res * 1.4 / 1000.0;
    return ESP_OK;
}

esp_err_t axp192_read_vbus(float *vbus) {
    uint8_t buf[2];
    esp_err_t err = axp192_read_reg(0x5A, buf, 2);
    if (err != ESP_OK) return err;
    uint16_t res = (buf[0] << 4) | (buf[1] & 0x0F);
    *vbus = res * 1.7 / 1000.0;
    return ESP_OK;
}

esp_err_t axp192_read_icharge(float *icharge) {
    uint8_t buf[2];
    esp_err_t err = axp192_read_reg(0x7A, buf, 2);
    if (err != ESP_OK) return err;
    uint16_t res = (buf[0] << 5) | (buf[1] & 0x1F);
    *icharge = res * 0.5;
    return ESP_OK;
}

esp_err_t axp192_read_idischarge(float *idischarge) {
    uint8_t buf[2];
    esp_err_t err = axp192_read_reg(0x7C, buf, 2);
    if (err != ESP_OK) return err;
    uint16_t res = (buf[0] << 5) | (buf[1] & 0x1F);
    *idischarge = res * 0.5;
    return ESP_OK;
}

esp_err_t axp192_read_temp(float *temp) {
    uint8_t buf[2];
    esp_err_t err = axp192_read_reg(0x5E, buf, 2);
    if (err != ESP_OK) return err;
    uint16_t res = (buf[0] << 4) | (buf[1] & 0x0F);
    *temp = -144.7 + (res * 0.1);
    return ESP_OK;
}
