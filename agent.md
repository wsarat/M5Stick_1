# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Project Overview

ESP-IDF project for the **Original M5Stick** (ESP32-PICO-D4) featuring the original gray OLED display. Turns the stick into a real-time automotive G-meter and sensor dashboard with ball/graph views, yaw-rate gauge, temperatures, battery monitoring, and BLE advertising. Uses FreeRTOS with multi-threaded architecture: IP5306 power management (I²C 0x75), SH1107 OLED driver (SPI, 64×128), and imu_sensor component for MPU9250/MPU6886. Framework version 6.1.

## Building and Running

```bash
# Set target (ESP32)
idf.py set-target esp32

# Build
idf.py build

# Flash and monitor (replace PORT with actual port, e.g., /dev/tty.usbserial-*)
idf.py -p PORT flash monitor2```

## Architecture

**Main component** (`main/main.c`):
- FreeRTOS-based main loop (no separate task, single-task architecture)
- Initializes IP5306 then SH1107 in sequence
- Updates display with battery level, charging status, and "M5Stick Original" text in a loop

**Component interfaces**:
- `ip5306`: Power management via I²C (address 0x75). Functions: `init()`, `get_battery_level()`, `is_charging()`, `is_full()`, `set_keep_awake()`
- `sh1107`: 64×128 OLED via SPI. Functions: `init()`, `clear()`, `update()`, `draw_string()`, `draw_battery()`.

## Hardware Reference

Always refer to `M5Stick.md` for pin mappings and display column offset requirements. Key pinout:
- SPI display (VSPI): SCLK=GPIO18, MOSI=GPIO23, CS=GPIO14, DC=GPIO27, RST=GPIO33
- I²C (shared): SDA=GPIO21, SCL=GPIO22 (for IP5306)
- Display column offset: **32** (set via byte 7, bit 5, value 0x20) - required for proper display positioning

## Testing

Run pytest with ESP-IDF target specification. Tests verify "Hello world!" output:
```bash
pytest -m pytest_hello_world.py -v -k "esp32 or linux or qemu"```

## ESP-IDF Specifics

- Build system: Standard ESP-IDF CMake structure with `main/` and `components/` directories
- Configuration: `sdkconfig` (project-specific), `sdkconfig.ci` (CI baseline)
- Flash/bootloader: `idf.py` handles flash operations; bootloader config is in `build/bootloader/`
- VSPI vs SPI: Use SPI3_HOST (VSPI) for display; SPI1 is reserved for flash

## Dependencies

- **Framework**: ESP-IDF 5.5.1
- **Target hardware**: ESP32-PICO-D4 with original 64×128 B/W OLED (SH1107)
- **Components**: FreeRTOS, I²C, SPI, ESP32 chip info, flash info, system utilities

## Development Notes

- **Power order**: IP5306 must be initialized first - it controls power to the system, including display
- **Display updates**: `sh1107_update()` is required after drawing; `sh1107_clear()` wipes the screen
- **Battery mapping** (IP5306 Reg 0x78, byte 7, high nibble): 100% at 0x00, 75% at 0x80, 50% at 0xC0, 25% at 0xE0, 0%/charging at 0xF02```

## Troubleshooting

- **Display not showing**: Check column offset (32), verify SPI pin mappings, ensure DC pin is correct (GPIO 27)
- **Power issues**: Check IP5306 initialization, verify I²C addresses, ensure SDA/SCL (21/22) are connected correctly
- **Upload failures**: Try different port, lower baud rate in menuconfig, check boot mode selection (GPI34 is pulled low on boot)

## Reference Examples
- **StickWatch** https://github.com/eggfly/StickWatch


## External Documentation
- **ESP-IDF V.5.5** https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/index.html# GEMINI.md - M5Stick (Original Gray) Project Context

## Project Overview
This is an ESP-IDF project designed for the **Original M5Stick (Gray)** featuring the ESP32-PICO-D4. It includes power management and display support specifically tuned for this vintage model.

### Core Technologies
- **Framework:** ESP-IDF v5.5.1
- **Target Hardware:** Original M5Stick (Gray)
- **PMIC:** IP5306 (I2C 0x75)
- **Display:** SH1107 OLED (64x128, SPI)

### Project Structure
- `main/main.c`: Primary application loop.
- `components/ip5306/`: Battery and power management.
- `components/sh1107/`: SPI-based OLED display driver.
- `M5Stick.md`: **Definitive hardware reference.** Use this file for all pin mappings and hardware specs.

## Building and Running
- **Set Target:** `idf.py set-target esp32`
- **Build & Flash:** `idf.py -p <PORT> flash monitor`

## Hardware Context (Reference M5Stick.md)
- **Display SPI:** SCLK=18, MOSI=23, CS=14, DC=27, RST=33.
- **I2C Shared Bus:** GPIO 21 (SDA), GPIO 22 (SCL).
- **Button A:** GPIO 35.
- **LED:** Blue (GPIO 19).

## Development Conventions
- **Hardware Truth:** Always refer to `M5Stick.md` for pin assignments.
- **Power:** IP5306 must be initialized to ensure system stability.
- **Display:** SH1107 uses a 64x128 resolution.

## Reference Examples
- **StickWatch** https://github.com/eggfly/StickWatch

## External Documentation
- **ESP-IDF V.5.5** https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/index.html