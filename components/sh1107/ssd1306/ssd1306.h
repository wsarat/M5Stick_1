#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define SSD1306_I2C_ADDR 0x3C
#define SSD1306_WIDTH    128
#define SSD1306_HEIGHT   64

esp_err_t ssd1306_init();
void ssd1306_clear();
void ssd1306_update();
void ssd1306_draw_pixel(int x, int y, bool color);
void ssd1306_draw_char(int x, int y, char c, bool color);
void ssd1306_draw_string(int x, int y, const char *str, bool color);
void ssd1306_draw_battery(int x, int y, int percent, bool charging, bool blink_state);

#endif // SSD1306_H
