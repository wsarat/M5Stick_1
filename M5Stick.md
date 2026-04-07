# M5Stick (Original Gray) - Ultimate Hardware Reference

This document is the definitive hardware reference and "cheat sheet" for the original M5Stick (B/W OLED version). It incorporates all findings from the `StickWatch` project and empirical debugging.

## Core Components
- **Microcontroller:** ESP32-PICO-D4 (Dual-core 240MHz, 4MB Flash, Wi-Fi/BT)
- **PMIC:** IP5306 (I2C Address: `0x75`)
- **Display:** 0.96" B/W OLED (64x128 resolution, SH1107 driver, SPI Interface)
- **IMU:** MPU6886 or MPU9250 (I2C Address: `0x68`)
- **RTC:** BM8563 (I2C Address: `0x51`)
- **LED:** Blue LED (GPIO 19, Active Low)
- **Button A:** GPIO 35 (Active Low)

## Pin Mapping (ESP32)

### SPI Interface (Display - SH1107)
| Function | Pin | Notes |
| :--- | :--- | :--- |
| **SCLK** | GPIO 18 | SPI Clock (Standard VSPI SCK) |
| **MOSI** | GPIO 23 | SPI Data (Standard VSPI MOSI) |
| **CS** | GPIO 14 | Chip Select (Active Low) |
| **DC** | GPIO 27 | Data / Command Selection |
| **RST** | GPIO 33 | Hardware Reset (Active Low) |

### I2C Interface (Internal Bus)
| Function | Pin | Notes |
| :--- | :--- | :--- |
| **SDA** | GPIO 21 | Shared: IP5306, RTC, IMU |
| **SCL** | GPIO 22 | Shared: IP5306, RTC, IMU |

### Peripherals
| Component | Pin | Function |
| :--- | :--- | :--- |
| **Button A** | GPIO 35 | Main Front Button (Active Low) |
| **Blue LED** | GPIO 19 | Internal Status LED (Active Low) |
| **IR LED** | GPIO 17 | Infrared Transmitter |
| **Mic CLK** | GPIO 0 | PDM Clock |
| **Mic DAT** | GPIO 34 | PDM Data |

## Technical Implementation Hints

### 1. Power Management (IP5306)
- **Keep Awake:** By default, IP5306 shuts down on "light load" (low current). To prevent this during ESP32 sleep/low-power modes, set `Register 0x00` Bit 1 to `1`.
- **Initialization:** Write `0x37` to `Reg 0x00` and `0x1D` to `Reg 0x01` for standard M5Stick behavior.
- **Battery Mapping (Reg 0x78):**
    - `0x00` -> 100%
    - `0x80` -> 75%
    - `0xC0` -> 50%
    - `0xE0` -> 25%
    - `0xF0` -> 0% (or charging/low)

### 2. Display (SH1107)
- **Column Offset:** The 64x128 panel is centered in the 128x128 RAM. You **MUST** apply a column offset of **32 (0x20)**. 
    - Set Column High Nibble to `0x12` (calculated as `0x10 | (32 >> 4)`).
- **Mirror Mode Fix:** To avoid mirrored or flipped text, use the following:
    - **Segment Remap:** `0xA0` (Normal)
    - **COM Scan Direction:** `0xC8` (Flipped)
- **Vertical Resolution:** The display is physically 128 pixels tall. In Page Mode, update all **16 pages** (Page 0 to 15).

### 3. SPI Configuration
- Use **SPI3_HOST** (standard VSPI) for the internal display to avoid conflicts with the flash on SPI1.
- Clock speed of **4MHz to 10MHz** is ideal for stability.

## Comparison Table: Gray vs. Orange (C)
| Feature | Original M5Stick (Gray) | M5StickC (Orange) |
| :--- | :--- | :--- |
| **Display Type** | **B/W OLED (SH1107)** | Color TFT (ST7735/ST7789) |
| **PMIC** | **IP5306** | AXP192 |
| **LED Color** | **Blue (GPIO 19)** | Red (GPIO 10) |
| **Button A** | **GPIO 35** | GPIO 37 |
| **Buzzer** | GPIO 26 | GPIO 2 |
