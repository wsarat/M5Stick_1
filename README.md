| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

# M5Stick (Original Gray) Automotive G-Meter & Sensors

This project is specifically configured for the **Original M5Stick** (ESP32-PICO-D4, SH1107 OLED). It features a multi-threaded architecture for real-time sensor processing and BLE broadcasting.

## Screen Modes
Press **Button A** (top button) to cycle through the 7 available screen modes:

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
- **Mode 5: Automotive G-Meter (Default)**
  - Top: Circular G-meter with 0.5G/1.0G markers and a **5-second trail** to track cornering/acceleration history.
  - Bottom: Absolute magnitude of total G-force (`sqrt(X^2 + Z^2)`) with a high-precision readout and intensity bar.
  - **Mapping:** `-Z` is Accelerate, `+Z` is Brake, `-X` is Left Turn, `+X` is Right Turn.
- **Mode 6: Yaw Rate Gauge & Graph (100 d/s)**
  - Top: Horizontal gauge showing real-time yaw rate (deg/s) using the IMU's Y-axis (vertical). Range extended to **100 deg/s**.
  - Bottom: Rolling graph of the yaw rate history.

## Features
- **Modular IMU Component:** All sensor processing (Kalman filtering, G-sensitivity compensation, low-pass filters) is encapsulated in the `imu_sensor` component.
- **Easy Configuration:** IMU parameters can be adjusted via the `imu_config_t` structure in `main.c`.
- **Multi-threaded Architecture:** 
  - `sensor_task`: 50Hz hardware sampling and IMU processing.
  - `display_task`: 20Hz smooth OLED refresh.
  - `ble_task`: 1Hz broadcast of processed sensor data.
  - `alarm_task`: Safety monitoring for dangerous yaw rates.
- **BLE Data:** Accelerometer data is broadcasted in the manufacturer-specific data field (8 bytes: `[FF FF][AX_HI AX_LO][AY_HI AY_LO][AZ_HI AZ_LO]`).

## Hardware
- **Device:** Original M5Stick (Gray version)
- **CPU:** ESP32-PICO-D4
- **Display:** 0.96" SH1107 OLED (64x128)
- **IMU:** MPU6886
- **PMIC:** IP5306

## How to use
1. Set the target to ESP32: `idf.py set-target esp32`
2. Build the project: `idf.py build`
3. Flash and monitor: `idf.py -p PORT flash monitor`


## Example folder contents

The project **hello_world** contains one source file in C language [main.c](main/main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt` files that provide set of directives and instructions describing the project's source files and targets (executable, library, or both).

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── pytest_hello_world.py      Python script used for automated testing
├── main
│   ├── CMakeLists.txt
│   └── hello_world_main.c
└── README.md                  This is the file you are currently reading
```

For more information on structure and contents of ESP-IDF projects, please refer to Section [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) of the ESP-IDF Programming Guide.

## Troubleshooting

* Program upload failure

    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-idf/issues)

We will get back to you as soon as possible.
