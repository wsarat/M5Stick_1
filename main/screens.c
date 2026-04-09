#include "screens.h"
#include "sh1107.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    float q; // process noise covariance
    float r; // measurement noise covariance
    float x; // value
    float p; // estimation error covariance
    float k; // kalman gain
} kalman_state_t;

static float  z_graph_buf[GRAPH_LEN];
static float  x_graph_buf[GRAPH_LEN];
static float  z_kalman_buf[GRAPH_LEN];
static float  x_kalman_buf[GRAPH_LEN];
static int    graph_idx = 0;

static kalman_state_t kalman_z;
static kalman_state_t kalman_x;
static SemaphoreHandle_t screens_mutex = NULL;

/* ---------------------------------------------------------------
 *  Kalman Filter Implementation
 * --------------------------------------------------------------- */
// ... (rest of the file)
static void kalman_init(kalman_state_t *state, float q, float r, float p, float initial_value) {
    state->q = q;
    state->r = r;
    state->p = p;
    state->x = initial_value;
}

static float kalman_update(kalman_state_t *state, float measurement) {
    // prediction update
    state->p = state->p + state->q;

    // measurement update
    state->k = state->p / (state->p + state->r);
    state->x = state->x + state->k * (measurement - state->x);
    state->p = (1 - state->k) * state->p;

    return state->x;
}

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

/* ---------------------------------------------------------------
 *  Layout Primitives
 * --------------------------------------------------------------- */
static void draw_ball_section(int y_offset, mpu6886_data_t *accel, int batt, bool charge, bool blink)
{
    int bx = (int)roundf(((accel->x + AXIS_RANGE) / (2 * AXIS_RANGE)) * (SCREEN_W - 1));
    int by = y_offset + (int)roundf(((accel->y + AXIS_RANGE) / (2 * AXIS_RANGE)) * (GRAPH_HEIGHT - 1));
    bx = (bx < 0) ? 0 : (bx > SCREEN_W-1 ? SCREEN_W-1 : bx);
    by = (by < y_offset) ? y_offset : (by > y_offset + 63 ? y_offset + 63 : by);

    sh1107_draw_circle(bx, by, 4, true);
    sh1107_draw_cross(bx, by, true);
    sh1107_draw_battery(48, y_offset + 2, batt, charge, blink);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", batt);
    sh1107_draw_string(30, y_offset + 2, buf, true);
    sh1107_draw_string(2, y_offset + 12, "M5Stick", true);
}

static void draw_z_graph(int y_offset, float z_now, float *buf, const char *label)
{
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

static void draw_x_graph_vertical(int y_offset, float x_now, float *buf, const char *label)
{
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

/* ---------------------------------------------------------------
 *  Public API
 * --------------------------------------------------------------- */
void screens_init(void)
{
    if (screens_mutex == NULL) {
        screens_mutex = xSemaphoreCreateMutex();
    }
    for (int i = 0; i < GRAPH_LEN; i++) {
        z_graph_buf[i] = 0.0f;
        x_graph_buf[i] = 0.0f;
        z_kalman_buf[i] = 0.0f;
        x_kalman_buf[i] = 0.0f;
    }
    graph_idx = 0;
    
    // Initialize Kalman filters: q (process noise), r (sensor noise), p (initial error), initial value
    kalman_init(&kalman_z, 0.01f, 0.1f, 1.0f, 0.0f);
    kalman_init(&kalman_x, 0.01f, 0.1f, 1.0f, 0.0f);
}

void screens_lock(void)
{
    if (screens_mutex) {
        xSemaphoreTake(screens_mutex, portMAX_DELAY);
    }
}

void screens_unlock(void)
{
    if (screens_mutex) {
        xSemaphoreGive(screens_mutex);
    }
}

void screens_update_data(mpu6886_data_t *accel)
{
    screens_lock();
    z_graph_buf[graph_idx] = accel->z;
    x_graph_buf[graph_idx] = accel->x;
    
    z_kalman_buf[graph_idx] = kalman_update(&kalman_z, accel->z);
    x_kalman_buf[graph_idx] = kalman_update(&kalman_x, accel->x);
    
    graph_idx = (graph_idx + 1) % GRAPH_LEN;
    screens_unlock();
}

void screens_draw(int mode, mpu6886_data_t *accel, int batt_level, bool charging, bool blink)
{
    screens_lock();
    sh1107_clear();
    draw_line(0, 64, 63, 64, true); // Middle separator

    switch (mode) {
        case 0: // Ball Top / Z-Graph Bottom
            draw_ball_section(0, accel, batt_level, charging, blink);
            draw_z_graph(64, accel->z, z_graph_buf, "Z");
            break;
        case 1: // Z-Graph Top / Ball Bottom
            draw_z_graph(0, accel->z, z_graph_buf, "Z");
            draw_ball_section(64, accel, batt_level, charging, blink);
            break;
        case 2: // Ball Top / X-Graph Bottom
            draw_ball_section(0, accel, batt_level, charging, blink);
            draw_x_graph_vertical(64, accel->x, x_graph_buf, "X");
            break;
        case 3: // X-Graph Top / Z-Graph Bottom
            draw_x_graph_vertical(0, accel->x, x_graph_buf, "X");
            draw_z_graph(64, accel->z, z_graph_buf, "Z");
            break;
        case 4: // Kalman X-Graph Top / Kalman Z-Graph Bottom
            draw_x_graph_vertical(0, kalman_x.x, x_kalman_buf, "KX");
            draw_z_graph(64, kalman_z.x, z_kalman_buf, "KZ");
            break;
        default:
            draw_x_graph_vertical(0, accel->x, x_graph_buf, "X");
            draw_z_graph(64, accel->z, z_graph_buf, "Z");
            break;
    }
    sh1107_update();
    screens_unlock();
}

