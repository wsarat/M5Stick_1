#include <string.h>
#include "ssd1306.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "SSD1306";

#define I2C_MASTER_NUM I2C_NUM_0

static uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

// Simple 5x7 Font
static const uint8_t font5x7[][5] = {
    ['0'-32] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
    ['1'-32] = {0x00, 0x42, 0x7F, 0x40, 0x00},
    ['2'-32] = {0x42, 0x61, 0x51, 0x49, 0x46},
    ['3'-32] = {0x21, 0x41, 0x45, 0x4B, 0x31},
    ['4'-32] = {0x18, 0x14, 0x12, 0x7F, 0x10},
    ['5'-32] = {0x27, 0x45, 0x45, 0x45, 0x39},
    ['6'-32] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
    ['7'-32] = {0x01, 0x71, 0x09, 0x05, 0x03},
    ['8'-32] = {0x36, 0x49, 0x49, 0x49, 0x36},
    ['9'-32] = {0x06, 0x49, 0x49, 0x29, 0x1E},
    ['%'-32] = {0x23, 0x13, 0x08, 0x64, 0x62},
    [' '-32] = {0x00, 0x00, 0x00, 0x00, 0x00}
};

static esp_err_t ssd1306_write_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_write_to_device(I2C_MASTER_NUM, SSD1306_I2C_ADDR, buf, 2, 100 / portTICK_PERIOD_MS);
}

esp_err_t ssd1306_init() {
    // I2C should already be initialized by ip5306_init, but we'll check if we need to do anything here.
    // Basic SSD1306 init sequence
    ssd1306_write_cmd(0xAE); // Display Off
    ssd1306_write_cmd(0xD5); // Set Display Clock Divide Ratio
    ssd1306_write_cmd(0x80);
    ssd1306_write_cmd(0xA8); // Set Multiplex Ratio
    ssd1306_write_cmd(0x3F);
    ssd1306_write_cmd(0xD3); // Set Display Offset
    ssd1306_write_cmd(0x00);
    ssd1306_write_cmd(0x40); // Set Display Start Line
    ssd1306_write_cmd(0x8D); // Charge Pump
    ssd1306_write_cmd(0x14);
    ssd1306_write_cmd(0x20); // Memory Addressing Mode
    ssd1306_write_cmd(0x00); // Horizontal Addressing Mode
    ssd1306_write_cmd(0xA1); // Set Segment Re-map
    ssd1306_write_cmd(0xC8); // Set COM Output Scan Direction
    ssd1306_write_cmd(0xDA); // Set COM Pins Hardware Configuration
    ssd1306_write_cmd(0x12);
    ssd1306_write_cmd(0x81); // Set Contrast Control
    ssd1306_write_cmd(0xCF);
    ssd1306_write_cmd(0xD9); // Set Pre-charge Period
    ssd1306_write_cmd(0xF1);
    ssd1306_write_cmd(0xDB); // Set VCOMH Deselect Level
    ssd1306_write_cmd(0x40);
    ssd1306_write_cmd(0xA4); // Entire Display On
    ssd1306_write_cmd(0xA6); // Set Normal Display
    ssd1306_write_cmd(0xAF); // Display On

    ssd1306_clear();
    ssd1306_update();
    return ESP_OK;
}

void ssd1306_clear() {
    memset(buffer, 0, sizeof(buffer));
}

void ssd1306_update() {
    for (uint8_t i = 0; i < 8; i++) {
        ssd1306_write_cmd(0xB0 + i);
        ssd1306_write_cmd(0x00);
        ssd1306_write_cmd(0x10);

        uint8_t data[SSD1306_WIDTH + 1];
        data[0] = 0x40;
        memcpy(&data[1], &buffer[i * SSD1306_WIDTH], SSD1306_WIDTH);
        i2c_master_write_to_device(I2C_MASTER_NUM, SSD1306_I2C_ADDR, data, SSD1306_WIDTH + 1, 100 / portTICK_PERIOD_MS);
    }
}

void ssd1306_draw_pixel(int x, int y, bool color) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) return;
    if (color) buffer[x + (y / 8) * SSD1306_WIDTH] |= (1 << (y % 8));
    else buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
}

void ssd1306_draw_char(int x, int y, char c, bool color) {
    if (c < 32 || c > 126) return;
    uint8_t index = c - 32;
    if (index >= sizeof(font5x7)/sizeof(font5x7[0])) return;
    for (int i = 0; i < 5; i++) {
        uint8_t line = font5x7[index][i];
        for (int j = 0; j < 7; j++) {
            if (line & (1 << j)) ssd1306_draw_pixel(x + i, y + j, color);
        }
    }
}

void ssd1306_draw_string(int x, int y, const char *str, bool color) {
    while (*str) {
        ssd1306_draw_char(x, y, *str++, color);
        x += 6;
    }
}

void ssd1306_draw_battery(int x, int y, int percent, bool charging, bool blink_state) {
    // Battery outline
    for (int i = 0; i < 12; i++) {
        ssd1306_draw_pixel(x + i, y, true);
        ssd1306_draw_pixel(x + i, y + 6, true);
    }
    for (int i = 0; i < 7; i++) {
        ssd1306_draw_pixel(x, y + i, true);
        ssd1306_draw_pixel(x + 11, y + i, true);
    }
    ssd1306_draw_pixel(x + 12, y + 2, true);
    ssd1306_draw_pixel(x + 12, y + 3, true);
    ssd1306_draw_pixel(x + 12, y + 4, true);

    if (charging) {
        if (blink_state) {
            // Draw a simple lightning bolt or fill
            for (int i = 2; i < 10; i++) {
                for (int j = 2; j < 5; j++) {
                    ssd1306_draw_pixel(x + i, y + j, true);
                }
            }
        }
    } else {
        // Fill based on percent
        int fill = (percent * 8) / 100;
        for (int i = 0; i < fill; i++) {
            for (int j = 2; j < 5; j++) {
                ssd1306_draw_pixel(x + 2 + i, y + j, true);
            }
        }
    }
}
