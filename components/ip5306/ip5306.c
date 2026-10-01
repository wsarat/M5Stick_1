#include "ip5306.h"
#include <stdio.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IP5306";

#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000

static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t i2c_dev_handle = NULL;

static esp_err_t i2c_init() {
    if (i2c_dev_handle) return ESP_OK;

    // ip5306_init() runs first, so create the bus here
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle);
    if (err != ESP_OK) {
        // Bus may already exist if another component initialized it first
        err = i2c_master_get_bus_handle(I2C_MASTER_NUM, &i2c_bus_handle);
        if (err != ESP_OK) return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = IP5306_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    return i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle);
}

static esp_err_t ip5306_read_reg(uint8_t reg, uint8_t *data) {
    return i2c_master_transmit_receive(i2c_dev_handle, &reg, 1, data, 1, 1000 / portTICK_PERIOD_MS);
}

static esp_err_t ip5306_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_transmit(i2c_dev_handle, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
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

    ip5306_dump_regs();
    return ESP_OK;
}

esp_err_t ip5306_get_battery_level(int *level) {
    uint8_t val;
    ip5306_write_reg(0x75, 0x78);
    esp_err_t err = ip5306_read_reg(0x75, &val);
    if (err != ESP_OK) return err;

    // Gauge nibble is ACTIVE-LOW (0 = LED on): 0x00 = all 4 LEDs = 100%
    switch (val & 0xF0) {
        case 0x00: *level = 100; break;
        case 0x80: *level = 75; break;
        case 0xC0: *level = 50; break;
        case 0xE0: *level = 25; break;
        case 0xF0: *level = 0; break;
        default: {
            // Count cleared bits in the LED nibble (4 x 25% steps)
            int on = 0;
            for (int b = 4; b < 8; b++) {
                if (!(val & (1 << b))) on++;
            }
            *level = (on > 4) ? 100 : on * 25;
            break;
        }
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

esp_err_t ip5306_dump_regs(void) {
    static const uint8_t regs[] = {
        0x00, 0x01, 0x02,             // SYS
        0x20, 0x21, 0x22, 0x23, 0x24, // charger config
        0x70, 0x71, 0x72,             // status
        0x75, 0x77, 0x78,             // battery view / key events / LED gauge
    };
    char line[144];
    size_t n = 0;
    for (size_t i = 0; i < sizeof(regs) && n < sizeof(line) - 12; i++) {
        uint8_t val;
        if (ip5306_read_reg(regs[i], &val) == ESP_OK) {
            n += snprintf(line + n, sizeof(line) - n, " %02X=%02X", regs[i], val);
        } else {
            n += snprintf(line + n, sizeof(line) - n, " %02X=--", regs[i]);
        }
    }
    ESP_LOGI(TAG, "regs:%s", line);
    return ESP_OK;
}

esp_err_t ip5306_get_key_events(uint8_t *raw, bool *short_press, bool *long_press, bool *double_press) {
    static uint8_t s_prev = 0;
    static bool s_init = false;
    if (short_press) *short_press = false;
    if (long_press) *long_press = false;
    if (double_press) *double_press = false;

    uint8_t val;
    esp_err_t err = ip5306_read_reg(0x77, &val);
    if (err != ESP_OK) return err;
    if (raw) *raw = val;

    // Register 0x77 bit 0 latches high on this hardware and cannot be
    // cleared by writing, so only report a change between polls.
    bool changed = s_init && (val != s_prev);
    s_prev = val;
    s_init = true;
    if (!changed) return ESP_OK;

    if (short_press) *short_press = (val & (1 << 0)) != 0;
    if (long_press)  *long_press  = (val & (1 << 1)) != 0;
    if (double_press) *double_press = (val & (1 << 2)) != 0;
    return ESP_OK;
}

// Approximate cell voltage per gauge level (midpoints of the IP5306
// charge/discharge voltage bands - an estimate, not a measurement).
int ip5306_estimate_voltage_mv(int level, bool charging) {
    static const int discharge_mv[5] = {3300, 3440, 3680, 3940, 4140};
    static const int charge_mv[5]    = {3400, 3510, 3750, 4000, 4160};

    int idx = level / 25;
    if (idx < 0) idx = 0;
    if (idx > 4) idx = 4;
    return charging ? charge_mv[idx] : discharge_mv[idx];
}
