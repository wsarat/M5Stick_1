# GEMINI.md - M5Stick (Original Gray) Project Context

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