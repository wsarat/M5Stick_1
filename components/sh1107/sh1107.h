#ifndef SH1107_H
#define SH1107_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define SH1107_WIDTH    64
#define SH1107_HEIGHT   128

// M5Stick Gray (Original) SPI Pins
#define SH1107_PIN_SCLK  18
#define SH1107_PIN_MOSI  23
#define SH1107_PIN_CS    14
#define SH1107_PIN_DC    27
#define SH1107_PIN_RST   33

esp_err_t sh1107_init();
void sh1107_clear();
void sh1107_update();
void sh1107_draw_pixel(int x, int y, bool color);
void sh1107_draw_char(int x, int y, char c, bool color);
void sh1107_draw_string(int x, int y, const char *str, bool color);
void sh1107_draw_battery(int x, int y, int percent, bool charging, bool blink_state);
void sh1107_draw_circle(int x, int y, int radius, bool color);
void sh1107_draw_cross(int x, int y, bool color);

#endif // SH1107_H