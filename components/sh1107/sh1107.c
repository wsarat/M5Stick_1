#include <string.h>
#include "sh1107.h"
#include "font5x7.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "SH1107";

static spi_device_handle_t spi;
static uint8_t buffer[SH1107_WIDTH * SH1107_HEIGHT / 8];

static void sh1107_write_cmd(uint8_t cmd) {
    gpio_set_level(SH1107_PIN_DC, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(spi, &t);
}

static void sh1107_write_data(uint8_t *data, size_t len) {
    gpio_set_level(SH1107_PIN_DC, 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(spi, &t);
}

esp_err_t sh1107_init() {
    ESP_LOGI(TAG, "Initializing SH1107 SPI (VSPI)...");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SH1107_PIN_DC) | (1ULL << SH1107_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    gpio_set_level(SH1107_PIN_RST, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(SH1107_PIN_RST, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = SH1107_PIN_MOSI,
        .sclk_io_num = SH1107_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SH1107_WIDTH * SH1107_HEIGHT / 8,
    };
    // SPI3_HOST = VSPI
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = SH1107_PIN_CS,
        .queue_size = 7,
    };
    spi_bus_add_device(SPI3_HOST, &devcfg, &spi);

    // Initialization sequence
    sh1107_write_cmd(0xAE); // Display Off
    sh1107_write_cmd(0x00); // Set lower column address
    sh1107_write_cmd(0x10); // Set higher column address
    sh1107_write_cmd(0x40); // Set display start line
    sh1107_write_cmd(0xB0); // Set page address
    sh1107_write_cmd(0x81); // Set contrast control
    sh1107_write_cmd(0x7F);
    sh1107_write_cmd(0xA0); // Segment remap (A0/A1 toggles horizontal mirror)
    sh1107_write_cmd(0xA6); // Normal display
    sh1107_write_cmd(0xA8); // Multiplex ratio
    sh1107_write_cmd(0x7F);
    sh1107_write_cmd(0xAD); // DC-DC control
    sh1107_write_cmd(0x81); // DC-DC ON
    sh1107_write_cmd(0xD3); // Display offset
    sh1107_write_cmd(0x00);
    sh1107_write_cmd(0xD5); // Display clock divide ratio
    sh1107_write_cmd(0x50);
    sh1107_write_cmd(0xD9); // Pre-charge period
    sh1107_write_cmd(0x22);
    sh1107_write_cmd(0xDB); // VCOMH deselect level
    sh1107_write_cmd(0x35);
    sh1107_write_cmd(0xC0); // COM scan direction (C0/C8 toggles vertical flip)
    sh1107_write_cmd(0xAF); // Display ON

    sh1107_clear();
    sh1107_update();
    return ESP_OK;
}

void sh1107_clear() {
    memset(buffer, 0, sizeof(buffer));
}

void sh1107_update() {
    // SH1107 is physically 64x128. Column offset of 32 is required to center the 64 pixels.
    // Column address = 32 (0x20)
    for (uint8_t i = 0; i < 16; i++) {
        sh1107_write_cmd(0xB0 + i); // Page address
        sh1107_write_cmd(0x00);     // Lower column address = 0 (offset 32 is in higher nibble)
        sh1107_write_cmd(0x12);     // Higher column address = 0x12 -> Column 0x20 (32)
        sh1107_write_data(&buffer[i * 64], 64);
    }
}

void sh1107_draw_pixel(int x, int y, bool color) {
    if (x < 0 || x >= SH1107_WIDTH || y < 0 || y >= SH1107_HEIGHT) return;
    if (color) buffer[x + (y / 8) * SH1107_WIDTH] |= (1 << (y % 8));
    else buffer[x + (y / 8) * SH1107_WIDTH] &= ~(1 << (y % 8));
}

void sh1107_draw_char(int x, int y, char c, bool color) {
    if (c < 32 || c > 126) return;
    uint8_t index = c - 32;
    for (int i = 0; i < 5; i++) {
        uint8_t line = font5x7[index][i];
        for (int j = 0; j < 7; j++) {
            if (line & (1 << j)) sh1107_draw_pixel(x + i, y + j, color);
        }
    }
}

void sh1107_draw_string(int x, int y, const char *str, bool color) {
    while (*str) {
        sh1107_draw_char(x, y, *str++, color);
        x += 6;
    }
}

void sh1107_draw_battery(int x, int y, int percent, bool charging, bool blink_state) {
    for (int i = 0; i < 12; i++) {
        sh1107_draw_pixel(x + i, y, true);
        sh1107_draw_pixel(x + i, y + 6, true);
    }
    for (int i = 0; i < 7; i++) {
        sh1107_draw_pixel(x, y + i, true);
        sh1107_draw_pixel(x + 11, y + i, true);
    }
    sh1107_draw_pixel(x + 12, y + 2, true);
    sh1107_draw_pixel(x + 12, y + 3, true);
    sh1107_draw_pixel(x + 12, y + 4, true);

    if (charging && blink_state) {
        for (int i = 2; i < 10; i++) {
            for (int j = 2; j < 5; j++) {
                sh1107_draw_pixel(x + i, y + j, true);
            }
        }
    } else if (!charging) {
        int fill = (percent * 8) / 100;
        for (int i = 0; i < fill; i++) {
            for (int j = 2; j < 5; j++) {
                sh1107_draw_pixel(x + 2 + i, y + j, true);
            }
        }
    }
}

/* New functions */
void sh1107_draw_circle(int x, int y, int radius, bool color) {
    // Draw circle perimeter using simple angle stepping
    const int steps = 100;
    for (int i = 0; i < steps; ++i) {
        double theta = i * (2.0 * M_PI / steps);
        int cx = x + (int)(radius * cos(theta));
        int cy = y + (int)(radius * sin(theta));
        if (cx >= 0 && cx < SH1107_WIDTH && cy >= 0 && cy < SH1107_HEIGHT) {
            sh1107_draw_pixel(cx, cy, color);
        }
    }
}

void sh1107_draw_cross(int x, int y, bool color) {
    // Vertical line
    for (int dy = -3; dy <= 3; ++dy) {
        sh1107_draw_pixel(x, y + dy, color);
    }
    // Horizontal line
    for (int dx = -3; dx <= 3; ++dx) {
        sh1107_draw_pixel(x + dx, y, color);
    }
}