# M5Stick (Original Gray) Automotive G-Meter & Sensors

FreeRTOS/ESP-IDF firmware for the **Original M5Stick** (ESP32-PICO-D4, 64x128 SH1107 B/W OLED). It turns the stick into a real-time automotive **G-meter** and sensor dashboard: ball/graph views of accelerometer axes, a G-meter with trail, yaw-rate gauge with audible alarm, die temperatures, battery monitoring, and BLE advertising of the accelerometer data.

- **Target:** ESP32 (`idf.py set-target esp32`), framework ESP-IDF 6.1
- **Boot screen:** Mode 7 (Temperatures); press **Button A** to cycle through the 8 screen modes
- **Hardware details:** see [M5Stick.md](M5Stick.md)

## Screen Modes

- **Mode 0: Ball & Z-Graph**
  - Top: 2D Accelerometer position (X/Y) represented as a moving ball.
  - Bottom: Rolling graph of the Z-axis (longitudinal/vertical).
- **Mode 1: Z-Graph & Ball**
  - Reverse of Mode 0.
- **Mode 2: Ball & X-Graph**
  - Top: 2D Accelerometer ball.
  - Bottom: Rolling graph of the X-axis (lateral).
- **Mode 3: X & Z Graphs**
  - Top: Rolling graph of X-axis.
  - Bottom: Rolling graph of Z-axis.
- **Mode 4: Kalman Filtered X & Z**
  - Smooth, noise-reduced rolling graphs of X and Z axes using a Kalman filter.
- **Mode 5: Automotive G-Meter**
  - Top: Circular G-meter with 0.5G/1.0G markers and a **5-second trail** (256 samples at 50Hz) to track cornering/acceleration history.
  - Bottom: Absolute magnitude of total G-force (`sqrt(X^2 + Z^2)`) with a high-precision readout and intensity bar.
  - **Mapping:** `-Z` is Accelerate, `+Z` is Brake, `-X` is Left Turn, `+X` is Right Turn.
- **Mode 6: Yaw Rate Gauge & Graph (100 d/s)**
  - Top: Horizontal gauge showing real-time yaw rate (deg/s) using the IMU's Y-axis (vertical). Range **+/- 100 deg/s** with 50 deg/s markers.
  - Bottom: Rolling graph of the yaw rate history.
- **Mode 7: Temperatures (boot screen)**
  - Top: MPU9250 die temperature with a -10C..80C scale bar.
  - Bottom: ESP32 CPU (die) temperature with the same scale bar. Sampled at 10Hz through the ROM temperature sensor with invalid-code filtering and a median-of-5 filter; `--.- C` is shown until the first valid sample.

## Features
- **Automotive G-Meter:** Circular gauge with trail + total-G readout (Mode 5).
- **Yaw-rate safety alarm:** warning beep above **50 deg/s**, critical double-beep + LED above **80 deg/s** (buzzer GPIO 26, blue LED GPIO 19).
- **Temperatures:** IMU die temp (MPU9250/MPU6886 conversion) and ESP32 die temp on one screen.
- **Battery monitoring:** IP5306 level/charging/full status polled at 50Hz, battery widget on the ball screens, plus an **estimated cell voltage** (e.g. `3.94V`) shown under the percent. All IP5306 registers are dumped to the log at boot.
- **Power button events:** presses of the side (IP5306 KEY) button are read from register `0x77` and logged (`IP5306 KEY: short/long/double`).
- **BLE broadcasting:** NimBLE, non-connectable advertising as `M5Stick-Gray`; manufacturer data (8 bytes) refreshed at 1Hz: `[FF FF][AX_HI AX_LO][AY_HI AY_LO][AZ_HI AZ_LO]`, accel in **milli-g** (g x 1000, little-endian int16).
- **Modular IMU Component:** All sensor processing (low-pass filter, Kalman filtering, G-sensitivity/yaw compensation) is encapsulated in the `imu_sensor` component; tuning lives in `imu_config_t` in `main.c`.
- **Multi-threaded Architecture:**

  | Task | Priority | Rate | Role |
  | :--- | :--- | :--- | :--- |
  | `alarm_task` | 6 (highest) | 10Hz | Yaw-rate buzzer/LED safety monitoring |
  | `sensor_task` | 5 | 50Hz | IMU sampling/processing, battery, button, CPU temp |
  | `display_task` | 4 | 20Hz | OLED refresh |
  | `ble_task` | 3 | 1Hz | BLE advertising data update |

  Shared state lives in `g_state` (guarded by `g_state_mutex`); graph/Kalman buffers live in `sensor_processor` (own lock).

## Project Structure

```
├── CMakeLists.txt               ESP-IDF project file (project: M5Stick_1)
├── M5Stick.md                   Hardware reference for the original M5Stick
├── main/
│   ├── main.c                   Init order, IMU config, task creation
│   ├── common_state.h           Shared state + Button A pin
│   ├── sensor_task.c            50Hz sensing, battery, mode switching, temp sampling
│   ├── display_task.c           20Hz display loop
│   ├── ble_task.c               NimBLE advertising (accelerometer mfg data)
│   ├── alarm_task.c             Yaw-rate alarm (buzzer + LED)
│   ├── sensor_processor.c       Graph buffers + Kalman filters
│   ├── screens.c                All 8 screen renderers
│   └── cpu_temp.c               ESP32 die temperature (ROM sensor + filtering)
└── components/
    ├── imu_sensor/              MPU9250/MPU6886 driver + filtering
    ├── ip5306/                  IP5306 power management driver (I2C)
    └── sh1107/                  SH1107 OLED driver (SPI, 5x7 font)
```

## How to use
1. Set the target to ESP32: `idf.py set-target esp32`
2. Build the project: `idf.py build`
3. Flash and monitor: `idf.py -p PORT flash monitor`

## Troubleshooting

* Display not showing: check the SH1107 column offset (32) and SPI pins (see [M5Stick.md](M5Stick.md)).
* Battery/power issues: IP5306 must be initialized first - it controls power to the system (`ip5306_init()` runs before the IMU on the shared I2C bus).
* Program upload failure:
    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.
