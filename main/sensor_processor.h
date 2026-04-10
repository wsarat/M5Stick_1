#ifndef SENSOR_PROCESSOR_H
#define SENSOR_PROCESSOR_H

#include "imu_sensor.h"

#define GRAPH_LEN         256

typedef struct {
    float z_graph[GRAPH_LEN];
    float x_graph[GRAPH_LEN];
    float z_kalman_graph[GRAPH_LEN];
    float x_kalman_graph[GRAPH_LEN];
    float yaw_rate_graph[GRAPH_LEN];
    int   graph_idx;
    float kalman_z_val;
    float kalman_x_val;
    float current_yaw_rate;
} processed_sensor_data_t;

/**
 * @brief Initialize sensor processor (Kalman filters, buffers).
 */
void sensor_processor_init(void);

/**
 * @brief Update processor with new processed IMU data.
 */
void sensor_processor_update(const imu_state_t *imu);

/**
 * @brief Lock processor for thread-safe data access.
 */
void sensor_processor_lock(void);

/**
 * @brief Unlock processor.
 */
void sensor_processor_unlock(void);

/**
 * @brief Get a pointer to the internal processed data structure.
 * Must be used within sensor_processor_lock/unlock.
 */
processed_sensor_data_t* sensor_processor_get_data(void);

#endif // SENSOR_PROCESSOR_H
