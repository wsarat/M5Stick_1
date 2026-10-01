#include "screens.h"
#include "sh1107.h"
#include "ip5306.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* ---------------------------------------------------------------
 *  Internal Helpers
 * --------------------------------------------------------------- */
static inline int z_to_graph_y(float z, int y_offset)
{
    float norm = (z + AXIS_RANGE) / (2 * AXIS_RANGE);
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;
    return y_offset + (int)roundf(norm * (GRAPH_HEIGHT - 1));
}

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

static void draw_dashed_line(int x0, int y0, int x1, int y1, int dash_len, bool color)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int dist = 0;

    while (true) {
        if ((dist / dash_len) % 2 == 0) {
            sh1107_draw_pixel(x0, y0, color);
        }
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
        dist++;
    }
}

/* ---------------------------------------------------------------
 *  Layout Primitives
 * --------------------------------------------------------------- */
static void draw_ball_section(int y_offset, imu_state_t *imu, int batt, bool charge, bool blink)
{
    int bx = (int)roundf(((imu->accel.x + AXIS_RANGE) / (2 * AXIS_RANGE)) * (SCREEN_W - 1));
    int by = y_offset + (int)roundf(((imu->accel.y + AXIS_RANGE) / (2 * AXIS_RANGE)) * (GRAPH_HEIGHT - 1));
    bx = (bx < 0) ? 0 : (bx > SCREEN_W-1 ? SCREEN_W-1 : bx);
    by = (by < y_offset) ? y_offset : (by > y_offset + 63 ? y_offset + 63 : by);

    sh1107_draw_circle(bx, by, 4, true);
    sh1107_draw_cross(bx, by, true);
    sh1107_draw_battery(48, y_offset + 2, batt, charge, blink);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", batt);
    sh1107_draw_string(30, y_offset + 2, buf, true);
    // Estimated cell voltage (4-level gauge midpoint, not a measurement)
    int batt_mv = ip5306_estimate_voltage_mv(batt, charge);
    snprintf(buf, sizeof(buf), "%d.%02dV", batt_mv / 1000, (batt_mv % 1000) / 10);
    sh1107_draw_string(2, y_offset + 12, buf, true);
}

static void draw_z_graph(int y_offset, float z_now, float *buf, int graph_idx, const char *label)
{
    // Draw center dashed line (0.0G)
    draw_dashed_line(0, y_offset + 32, 63, y_offset + 32, 2, true);

    for (int i = 0; i < GRAPH_LEN - 1; i++) {
        int idx1 = (graph_idx + i) % GRAPH_LEN;
        int idx2 = (graph_idx + i + 1) % GRAPH_LEN;
        int y1 = z_to_graph_y(buf[idx1], y_offset);
        int y2 = z_to_graph_y(buf[idx2], y_offset);
        draw_line(i, y1, i + 1, y2, true);
    }
    char buf_str[24];
    snprintf(buf_str, sizeof(buf_str), "%s:%.2f", label, z_now);
    sh1107_draw_string(2, y_offset + 56, buf_str, true);
}

static void draw_x_graph_vertical(int y_offset, float x_now, float *buf, int graph_idx, const char *label)
{
    // Draw center dashed line (0.0G)
    draw_dashed_line(32, y_offset, 32, y_offset + 63, 2, true);

    for (int i = 0; i < GRAPH_LEN - 1; i++) {
        int idx1 = (graph_idx + i) % GRAPH_LEN;
        int idx2 = (graph_idx + i + 1) % GRAPH_LEN;
        float nx1 = (buf[idx1] + AXIS_RANGE) / (2 * AXIS_RANGE);
        float nx2 = (buf[idx2] + AXIS_RANGE) / (2 * AXIS_RANGE);
        draw_line((int)roundf(nx1 * 63), y_offset + i, (int)roundf(nx2 * 63), y_offset + i + 1, true);
    }
    char buf_str[24];
    snprintf(buf_str, sizeof(buf_str), "%s:%.2f", label, x_now);
    sh1107_draw_string(2, y_offset + 56, buf_str, true);
}

static void draw_g_meter(int y_offset, float x_now, float z_now, float *x_buf, float *z_buf, int graph_idx)
{
    int cx = 32;
    int cy = y_offset + 32;
    int scale = 24; // 1.0G = 24 pixels

    // Draw background circles
    sh1107_draw_circle(cx, cy, scale/2, true); // 0.5G
    sh1107_draw_circle(cx, cy, scale, true);   // 1.0G
    draw_dashed_line(cx - scale, cy, cx + scale, cy, 2, true);
    draw_dashed_line(cx, cy - scale, cx, cy + scale, 2, true);

    // Draw trail (5 seconds = 250 samples at 50Hz)
    // We walk backwards from graph_idx
    for (int i = 1; i < 250 && i < GRAPH_LEN; i++) {
        int idx = (graph_idx - i + GRAPH_LEN) % GRAPH_LEN;
        // User mapping: -X=Left, +X=Right, -Z=Acc, +Z=Brk
        // On screen: x = cx + X*scale, y = cy + Z*scale
        int tx = cx + (int)roundf(x_buf[idx] * scale);
        int ty = cy + (int)roundf(z_buf[idx] * scale);
        if (tx >= 0 && tx < SCREEN_W && ty >= y_offset && ty < y_offset + 64) {
            sh1107_draw_pixel(tx, ty, true);
        }
    }

    // Draw current G dot (larger)
    int dot_x = cx + (int)roundf(x_now * scale);
    int dot_y = cy + (int)roundf(z_now * scale);
    sh1107_draw_circle(dot_x, dot_y, 2, true);
}

static void draw_total_g(int y_offset, float x, float z)
{
    float total_g = sqrtf(x*x + z*z);
    char buf[32];
    
    sh1107_draw_string(2, y_offset + 5, "TOTAL G", true);
    
    snprintf(buf, sizeof(buf), "%.3f", total_g);
    sh1107_draw_string(10, y_offset + 25, buf, true); // Large text placeholder

    // Simple Bar
    int bar_w = (int)(total_g * 60);
    if (bar_w > 60) bar_w = 60;
    for (int i=0; i<8; i++) {
        draw_line(2, y_offset + 45 + i, 2 + bar_w, y_offset + 45 + i, true);
    }
}

static void draw_yaw_rate_gauge(int y_offset, float rate)
{
    int cx = 32;
    int cy = y_offset + 32;
    float scale = 0.3f; // 100 deg/s = 30 pixels (fills the side)

    // Gauge background (Ticks)
    draw_line(cx-30, cy-5, cx-30, cy+5, true); // L marker (100)
    draw_line(cx+30, cy-5, cx+30, cy+5, true); // R marker (100)
    draw_dashed_line(cx, cy-15, cx, cy+15, 2, true); // Zero
    draw_line(cx-15, cy-2, cx-15, cy+2, true); // 50 deg/s marker
    draw_line(cx+15, cy-2, cx+15, cy+2, true); // 50 deg/s marker

    // Bar
    int bar_w = (int)(rate * scale);
    if (bar_w > 30) bar_w = 30;
    if (bar_w < -30) bar_w = -30;
    
    for (int i=0; i<4; i++) {
        draw_line(cx, cy - 2 + i, cx + bar_w, cy - 2 + i, true);
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f d/s", rate);
    sh1107_draw_string(2, y_offset + 2, buf, true);
}

static void draw_yaw_rate_graph(int y_offset, float *buf, int graph_idx)
{
    // Draw center line (0 deg/s)
    draw_dashed_line(0, y_offset + 32, 63, y_offset + 32, 2, true);

    for (int i = 0; i < 250 && i < GRAPH_LEN - 1; i++) {
        int idx1 = (graph_idx - i + GRAPH_LEN) % GRAPH_LEN;
        int idx2 = (graph_idx - i - 1 + GRAPH_LEN) % GRAPH_LEN;
        
        // Scale 100 deg/s to 32 pixels
        int y1 = y_offset + 32 + (int)(buf[idx1] * (32.0f / 100.0f));
        int y2 = y_offset + 32 + (int)(buf[idx2] * (32.0f / 100.0f));

        y1 = (y1 < y_offset) ? y_offset : (y1 > y_offset + 63 ? y_offset + 63 : y1);
        y2 = (y2 < y_offset) ? y_offset : (y2 > y_offset + 63 ? y_offset + 63 : y2);

        int x1 = 63 - (i * 64 / 250);
        int x2 = 63 - ((i + 1) * 64 / 250);
        
        draw_line(x1, y1, x2, y2, true);
    }
    sh1107_draw_string(2, y_offset + 2, "100 deg/s", true);
}

static void draw_temp_section(int y_offset, const char *label, float temp_c)
{
    char buf[24];

    sh1107_draw_string(2, y_offset + 4, label, true);

    if (isnan(temp_c)) {
        snprintf(buf, sizeof(buf), "--.- C");
        temp_c = -10.0f; // empty bar
    } else {
        snprintf(buf, sizeof(buf), "%.1f C", temp_c);
    }
    sh1107_draw_string(2, y_offset + 20, buf, true);

    // Scale bar: -10C..80C mapped to 0..60 px
    int bar_w = (int)((temp_c + 10.0f) * 60.0f / 90.0f);
    if (bar_w < 0) bar_w = 0;
    if (bar_w > 60) bar_w = 60;
    for (int i = 0; i < 6; i++) {
        draw_line(2, y_offset + 34 + i, 2 + bar_w, y_offset + 34 + i, true);
    }

    sh1107_draw_string(2, y_offset + 46, "-10", true);
    sh1107_draw_string(46, y_offset + 46, "80", true);
}

/* ---------------------------------------------------------------
 *  Public API
 * --------------------------------------------------------------- */
void screens_init(void)
{
    // No longer needs internal state/mutex for processing
}

void screens_draw(int mode, imu_state_t *imu, processed_sensor_data_t *data, int batt_level, bool charging, bool blink, float cpu_temp_c)
{
    sh1107_clear();
    draw_line(0, 64, 63, 64, true); // Middle separator

    switch (mode) {
        case 0: // Ball Top / Z-Graph Bottom
            draw_ball_section(0, imu, batt_level, charging, blink);
            draw_z_graph(64, imu->accel.z, data->z_graph, data->graph_idx, "Z");
            break;
        case 1: // Z-Graph Top / Ball Bottom
            draw_z_graph(0, imu->accel.z, data->z_graph, data->graph_idx, "Z");
            draw_ball_section(64, imu, batt_level, charging, blink);
            break;
        case 2: // Ball Top / X-Graph Bottom
            draw_ball_section(0, imu, batt_level, charging, blink);
            draw_x_graph_vertical(64, imu->accel.x, data->x_graph, data->graph_idx, "X");
            break;
        case 3: // X-Graph Top / Z-Graph Bottom
            draw_x_graph_vertical(0, imu->accel.x, data->x_graph, data->graph_idx, "X");
            draw_z_graph(64, imu->accel.z, data->z_graph, data->graph_idx, "Z");
            break;
        case 4: // Kalman X-Graph Top / Kalman Z-Graph Bottom
            draw_x_graph_vertical(0, data->kalman_x_val, data->x_kalman_graph, data->graph_idx, "KX");
            draw_z_graph(64, data->kalman_z_val, data->z_kalman_graph, data->graph_idx, "KZ");
            break;
        case 5: // G-Meter Top / Absolute G Bottom
            draw_g_meter(0, imu->accel.x, imu->accel.z, data->x_graph, data->z_graph, data->graph_idx);
            draw_total_g(64, imu->accel.x, imu->accel.z);
            break;
        case 6: // Yaw Rate Gauge Top / Yaw Rate Graph Bottom
            draw_yaw_rate_gauge(0, data->current_yaw_rate);
            draw_yaw_rate_graph(64, data->yaw_rate_graph, data->graph_idx);
            break;
        case 7: // IMU Temperature Top / CPU Temperature Bottom
            draw_temp_section(0, "IMU TEMP", imu->temp_c);
            draw_temp_section(64, "CPU TEMP", cpu_temp_c);
            break;
        default:
            draw_x_graph_vertical(0, imu->accel.x, data->x_graph, data->graph_idx, "X");
            draw_z_graph(64, imu->accel.z, data->z_graph, data->graph_idx, "Z");
            break;
    }
    sh1107_update();
}
