#include "ip5306.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "IP5306";

#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
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
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    if (err == ESP_ERR_INVALID_STATE) return ESP_OK;
    return err;
}

static esp_err_t ip5306_read_reg(uint8_t reg, uint8_t *data) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, IP5306_I2C_ADDR, &reg, 1, data, 1, 1000 / portTICK_PERIOD_MS);
}

static esp_err_t ip5306_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, IP5306_I2C_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

esp_err_t ip5306_init() {
    esp_err_t err = i2c_init();
    if (err != ESP_OK) return err;

    uint8_t val;
    err = ip5306_read_reg(0x00, &val);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IP5306 not responding at 0x%02X", IP5306_I2C_ADDR);
        return err;
    }
    ESP_LOGI(TAG, "IP5306 detected! Reg 0x00: 0x%02X", val);

    ip5306_write_reg(0x00, 0x37); 
    ip5306_write_reg(0x01, 0x1D); 

    return ESP_OK;
}

esp_err_t ip5306_get_battery_level(int *level) {
    uint8_t val;
    ip5306_write_reg(0x75, 0x78); 
    esp_err_t err = ip5306_read_reg(0x75, &val);
    if (err != ESP_OK) return err;

    switch (val & 0xF0) {
        case 0xF0: *level = 100; break;
        case 0xE0: *level = 75; break;
        case 0xC0: *level = 50; break;
        case 0xB0: *level = 25; break;
        // default:   *level = 0; break;
        default:   *level = val; break;
    }

    return ESP_OK;
}

esp_err_t ip5306_is_charging(bool *charging) {
    uint8_t val;
    esp_err_t err = ip5306_read_reg(0x70, &val);
    if (err != ESP_OK) return err;
    *charging = (val & (1 << 3)) ? true : false;
    return ESP_OK;
}

esp_err_t ip5306_is_full(bool *full) {
    uint8_t val;
    esp_err_t err = ip5306_read_reg(0x71, &val);
    if (err != ESP_OK) return err;
    *full = (val & (1 << 3)) ? true : false;
    return ESP_OK;
}

esp_err_t ip5306_set_keep_awake(bool enable) {
    uint8_t val;
    ip5306_read_reg(0x00, &val);
    if (enable) val |= 0x02;
    else val &= ~0x02;
    return ip5306_write_reg(0x00, val);
}
