#ifndef SCREENS_H
#define SCREENS_H

#include "mpu6886.h"
#include <stdbool.h>

#define SCREEN_W          64
#define SCREEN_H          128
#define GRAPH_HEIGHT      64
#define GRAPH_LEN         64
#define AXIS_RANGE        1.0f

/**
 * @brief Initialize screen buffers and state.
 */
void screens_init(void);

/**
 * @brief Update internal graph buffers with new accelerometer data.
 */
void screens_update_data(mpu6886_data_t *accel);

/**
 * @brief Draw the selected screen mode to the SH1107 buffer.
 * 
 * @param mode The screen mode index (0-3).
 * @param accel Current accelerometer data for real-time elements (like the ball).
 * @param batt_level Battery level percentage.
 * @param charging Charging status.
 * @param blink Blink state for battery indicator.
 */
void screens_draw(int mode, mpu6886_data_t *accel, int batt_level, bool charging, bool blink);

#endif // SCREENS_H
