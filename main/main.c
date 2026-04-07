/* ---------------------------------------------------------------
 *  M5Stick‑Original – split‑screen demo
 *  Top  : moving ball that follows MPU‑6886 (ax, ay) in the range
 *         –1.5 g … +1.5 g  →  mapped to (0 … 63) × (0 … 63)
 *  Bottom: scrolling line‑graph of the Z‑axis acceleration.
 *
 *  The code uses only the existing SH1107 driver (sh1107.c / .h).
 * --------------------------------------------------------------- */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "ip5306.h"
#include "sh1107.h"
#include "mpu6886.h"

/* ---------------------------------------------------------------
 *  Configuration constants
 * --------------------------------------------------------------- */
#define SCREEN_W          64
#define SCREEN_H          128
#define GRAPH_HEIGHT      (SCREEN_H / 2)          // 64 rows for the graph
#define GRAPH_LEN         64                      // number of points kept
#define AXIS_RANGE        2.0f                    // ±2.0 g visible range

/* ---------------------------------------------------------------
 *  Helper: map a signed acceleration value (‑AXIS_RANGE … +AXIS_RANGE)
 *          to a pixel coordinate inside the *graph* area.
 *
 *  The graph occupies the lower half of the screen (rows 64 … 127).
 * --------------------------------------------------------------- */
static inline int z_to_graph_y(float z)
{
    /* Map [-AXIS_RANGE , +AXIS_RANGE] → [64 , 127] */
    float norm = (z + AXIS_RANGE) / (2 * AXIS_RANGE);   // 0 … 1
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;
    int y = 64 + (int)roundf(norm * (GRAPH_HEIGHT - 1));
    return y;
}

/* ---------------------------------------------------------------
 *  Simple line‑drawer – draws a line between (x0,y0) and (x1,y1)
 * --------------------------------------------------------------- */
static void draw_line(int x0, int y0, int x1, int y1, bool color)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        sh1107_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* ---------------------------------------------------------------
 *  Global state
 * --------------------------------------------------------------- */
static bool   blink = false;                 // used for the battery‑blink
static float  graph_buf[GRAPH_LEN];          // circular buffer for Z‑accel
static int    graph_idx = 0;                  // write pointer into the buffer

/* ---------------------------------------------------------------
 *  Main entry point
 * --------------------------------------------------------------- */
void app_main(void)
{
    printf("M5Stick (Original) Initializing...\n");

    if (ip5306_init() != ESP_OK) {
        printf("Failed to init IP5306!\n");
    }
    if (mpu6886_init() != ESP_OK) {
        printf("Failed to init MPU6886!\n");
    }
    if (sh1107_init() != ESP_OK) {
        printf("Failed to init SH1107!\n");
    }

    // Initialize graph buffer
    for (int i = 0; i < GRAPH_LEN; i++) graph_buf[i] = 0.0f;

    // Subscribe to TWDT
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 5000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&twdt_config);
    esp_task_wdt_add(NULL);

    while (1) {
        /* 1. Read sensors */
        mpu6886_data_t accel, gyro;
        int batt_level = 0;
        bool charging = false;
        ip5306_get_battery_level(&batt_level);
        ip5306_is_charging(&charging);
        mpu6886_get_accel(&accel);
        mpu6886_get_gyro(&gyro);

        /* Update graph buffer */
        graph_buf[graph_idx] = accel.z;
        graph_idx = (graph_idx + 1) % GRAPH_LEN;

        /* 2. Clear buffer */
        sh1107_clear();

        /* 3. TOP HALF: Ball */
        int ball_x = (int)roundf(((accel.x + AXIS_RANGE) / (2 * AXIS_RANGE)) * (SCREEN_W - 1));
        int ball_y = (int)roundf(((accel.y + AXIS_RANGE) / (2 * AXIS_RANGE)) * (GRAPH_HEIGHT - 1));
        
        /* Clamp to top half bounds */
        ball_x = (ball_x < 0) ? 0 : (ball_x > SCREEN_W-1 ? SCREEN_W-1 : ball_x);
        ball_y = (ball_y < 0) ? 0 : (ball_y > 63 ? 63 : ball_y);

        sh1107_draw_circle(ball_x, ball_y, 4, true);
        sh1107_draw_cross(ball_x, ball_y, true);

        /* Battery & Status (Top) */
        sh1107_draw_battery(48, 2, batt_level, charging, blink);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", batt_level);
        sh1107_draw_string(30, 2, buf, true);
        sh1107_draw_string(2, 2, "M5Stick", true);

        /* Telemetry */
        snprintf(buf, sizeof(buf), "Z:%.2f", accel.z);
        sh1107_draw_string(2, 120, buf, true);

        /* 4. BOTTOM HALF: Scrolling Graph */
        // Draw a separator line
        draw_line(0, 64, 63, 64, true);

        for (int i = 0; i < GRAPH_LEN - 1; i++) {
            int idx1 = (graph_idx + i) % GRAPH_LEN;
            int idx2 = (graph_idx + i + 1) % GRAPH_LEN;
            int y1 = z_to_graph_y(graph_buf[idx1]);
            int y2 = z_to_graph_y(graph_buf[idx2]);
            draw_line(i, y1, i + 1, y2, true);
        }

        /* 5. Update display */
        sh1107_update();

        /* 6. Maintenance */
        esp_task_wdt_reset();
        blink = !blink;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}