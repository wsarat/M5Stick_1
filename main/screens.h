#ifndef SCREENS_H
#define SCREENS_H

#include "imu_sensor.h"
#include "sensor_processor.h"
#include <stdbool.h>

#define SCREEN_W          64
#define SCREEN_H          128
#define GRAPH_HEIGHT      64
#define AXIS_RANGE        1.0f

/**
 * @brief Initialize screen buffers and state.
 */
void screens_init(void);

/**
 * @brief Draw the selected screen mode.
 * 
 * @param mode The screen mode index (0-4).
 * @param accel Current accelerometer data for real-time elements.
 * @param data Pointer to processed sensor data (graphs, Kalman).
 * @param batt_level Battery level percentage.
 * @param charging Charging status.
 * @param blink Blink state for battery indicator.
 */
void screens_draw(int mode, imu_state_t *imu, processed_sensor_data_t *data, int batt_level, bool charging, bool blink);

#endif // SCREENS_H
