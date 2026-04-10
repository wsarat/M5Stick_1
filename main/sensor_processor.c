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
    kalman_init(&kalman_z, 0.01f, 0.1f, 1.0f, 0.0f);
    kalman_init(&kalman_x, 0.01f, 0.1f, 1.0f, 0.0f);
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

void sensor_processor_update(const imu_state_t *imu)
{
    sensor_processor_lock();
    
    // Store data in rolling graphs
    g_processed_data.z_graph[g_processed_data.graph_idx] = imu->accel.z;
    g_processed_data.x_graph[g_processed_data.graph_idx] = imu->accel.x;
    
    // Secondary Kalman filtering for "Smooth" screen mode
    g_processed_data.kalman_z_val = kalman_update(&kalman_z, imu->accel.z);
    g_processed_data.kalman_x_val = kalman_update(&kalman_x, imu->accel.x);
    
    g_processed_data.z_kalman_graph[g_processed_data.graph_idx] = g_processed_data.kalman_z_val;
    g_processed_data.x_kalman_graph[g_processed_data.graph_idx] = g_processed_data.kalman_x_val;
    
    // Yaw rate from IMU sensor component
    g_processed_data.current_yaw_rate = imu->yaw_rate;
    g_processed_data.yaw_rate_graph[g_processed_data.graph_idx] = imu->yaw_rate;
    
    // Increment index
    g_processed_data.graph_idx = (g_processed_data.graph_idx + 1) % GRAPH_LEN;
    
    sensor_processor_unlock();
}

processed_sensor_data_t* sensor_processor_get_data(void)
{
    return &g_processed_data;
}
