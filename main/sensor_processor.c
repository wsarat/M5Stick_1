#include "sensor_processor.h"
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    float q; // process noise covariance
    float r; // measurement noise covariance
    float x; // value
    float p; // estimation error covariance
    float k; // kalman gain
} kalman_state_t;

static kalman_state_t kalman_z;
static kalman_state_t kalman_x;
static processed_sensor_data_t g_processed_data;
static SemaphoreHandle_t processor_mutex = NULL;

/* ---------------------------------------------------------------
 *  Kalman Filter Implementation
 * --------------------------------------------------------------- */
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
 *  Public API
 * --------------------------------------------------------------- */
void sensor_processor_init(void)
{
    if (processor_mutex == NULL) {
        processor_mutex = xSemaphoreCreateMutex();
    }
    
    memset(&g_processed_data, 0, sizeof(g_processed_data));
    g_processed_data.graph_idx = 0;
    
    // Initialize Kalman filters: q (process noise), r (sensor noise), p (initial error), initial value
    kalman_init(&kalman_z, 0.01f, 0.5f, 1.0f, 0.0f);
    kalman_init(&kalman_x, 0.01f, 0.5f, 1.0f, 0.0f);
}

void sensor_processor_lock(void)
{
    if (processor_mutex) {
        xSemaphoreTake(processor_mutex, portMAX_DELAY);
    }
}

void sensor_processor_unlock(void)
{
    if (processor_mutex) {
        xSemaphoreGive(processor_mutex);
    }
}

static float g_yaw_rate_filter = 0.0f;

// Compensation factors (deg/s per G)
// These may need fine-tuning based on physical mounting flex
#define YAW_COMP_X  1.8f  // Adjusts for Lateral G leakage
#define YAW_COMP_Z  0.5f  // Adjusts for Accel/Brake G leakage

void sensor_processor_update(mpu6886_data_t *accel, mpu6886_data_t *gyro)
{
    sensor_processor_lock();
    
    // Current raw values
    g_processed_data.z_graph[g_processed_data.graph_idx] = accel->z;
    g_processed_data.x_graph[g_processed_data.graph_idx] = accel->x;
    
    // Current Kalman values
    g_processed_data.kalman_z_val = kalman_update(&kalman_z, accel->z);
    g_processed_data.kalman_x_val = kalman_update(&kalman_x, accel->x);
    
    // Kalman graph buffers
    g_processed_data.z_kalman_graph[g_processed_data.graph_idx] = g_processed_data.kalman_z_val;
    g_processed_data.x_kalman_graph[g_processed_data.graph_idx] = g_processed_data.kalman_x_val;
    
    // 1. Compensate for G-Sensitivity (Linear Acceleration Interference)
    // We subtract the interference caused by lateral (X) and longitudinal (Z) forces
    float raw_yaw_rate = gyro->y;
    float compensated_yaw_rate = raw_yaw_rate - (accel->x * YAW_COMP_X) - (accel->z * YAW_COMP_Z);

    // 2. Low-pass filter to remove vibration noise
    g_yaw_rate_filter = (g_yaw_rate_filter * 0.85f) + (compensated_yaw_rate * 0.15f);
    
    // 3. Apply a small deadzone to ignore static drift
    if (fabsf(g_yaw_rate_filter) < 0.5f) g_yaw_rate_filter = 0.0f;

    g_processed_data.current_yaw_rate = g_yaw_rate_filter;
    g_processed_data.yaw_rate_graph[g_processed_data.graph_idx] = g_processed_data.current_yaw_rate;
    
    // Increment index
    g_processed_data.graph_idx = (g_processed_data.graph_idx + 1) % GRAPH_LEN;
    
    sensor_processor_unlock();
}

processed_sensor_data_t* sensor_processor_get_data(void)
{
    return &g_processed_data;
}
