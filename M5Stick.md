# M5Stick (Original Gray) - Ultimate Hardware Reference

This document is the definitive hardware reference and "cheat sheet" for the original M5Stick (B/W OLED version). It incorporates all findings from the `StickWatch` project and empirical debugging, and is kept in sync with the firmware in this repository.

## Core Components
- **Microcontroller:** ESP32-PICO-D4 (Dual-core 240MHz, 4MB SPI Flash, Wi-Fi/BT)
- **PMIC:** IP5306 (I2C Address: `0x75`)
- **Display:** 0.96" B/W OLED (64x128 resolution, SH1107 driver, SPI Interface)
- **IMU:** MPU9250 (I2C Address: `0x68`, WHO_AM_I register `0x75` -> `0x71`; MPU6886 `0x38` also accepted by the driver)
- **RTC:** BM8563 (I2C Address: `0x51`) - present on the board, not used by this firmware
- **LED:** Blue LED (GPIO 19, Active Low)
- **Buzzer:** GPIO 26 (driven via LEDC)
- **Button A:** GPIO 35 (Active Low, **input-only pin**)
- **IR LED:** GPIO 17 (transmitter)
- **Mic:** PDM (CLK GPIO 0, DAT GPIO 34)

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

- Bus speed: **100 kHz**, internal pull-ups enabled.
- Bus ownership: `ip5306_init()` runs first and creates the bus; the IMU driver reuses it via `i2c_master_get_bus_handle()` (no second bus instance).

### Peripherals
| Component | Pin | Function |
| :--- | :--- | :--- |
| **Button A** | GPIO 35 | Main Front Button (Active Low, input-only) |
| **Blue LED** | GPIO 19 | Internal Status LED (Active Low) |
| **Buzzer** | GPIO 26 | Warning buzzer (LEDC, 2kHz base) |
| **IR LED** | GPIO 17 | Infrared Transmitter |
| **Mic CLK** | GPIO 0 | PDM Clock |
| **Mic DAT** | GPIO 34 | PDM Data |

## Technical Implementation Hints

### 1. Power Management (IP5306)
- **Keep Awake:** By default, IP5306 shuts down on "light load" (low current). To prevent this during ESP32 sleep/low-power modes, set `Register 0x00` Bit 1 to `1`.
- **Initialization:** Write `0x37` to `Reg 0x00` and `0x1D` to `Reg 0x01` for standard M5Stick behavior (done by `ip5306_init()`).
- **Battery Level:** firmware writes `0x78` to `Reg 0x75`, then reads `Reg 0x75` and decodes the **high nibble**:
    - `0x00` -> 100%
    - `0x80` -> 75%
    - `0xC0` -> 50%
    - `0xE0` -> 25%
    - `0xF0` -> 0% (or charging/low)
- **Charging status:** `Reg 0x70`, Bit 3 (1 = charging).
- **Charging full:** `Reg 0x71`, Bit 3 (1 = full).
- **Load status:** `Reg 0x72`, Bit 2 (1 = light load, 0 = heavy load).
- **Power button events:** `Reg 0x77` Bits 0/1/2 = short/long/double press of the IP5306 KEY (the side power button - the only way for the ESP32 to observe it). Bit 0 latches high and **cannot be cleared by writing** on this hardware, so the firmware polls at 50Hz and reports only *changes* between polls (rate-limited log).
- **LED gauge:** `Reg 0x78` Bits 4-7 = LED1-4 (the 4-level battery indicator).
- **Register map (used by the IP5306-I2C variant):** `0x00-0x02` (SYS), `0x20-0x24` (charger), `0x70-0x72`, `0x77`, `0x78` (read/status). `ip5306_dump_regs()` logs all of them at boot.
- **No voltage/current ADC:** the I2C interface exposes only the 4-level gauge - actual cell voltage cannot be read. Firmware shows an **estimated** voltage (band midpoint per level, charge/discharge bands differ) on the ball screens.

### 2. Display (SH1107)
- **Column Offset:** The 64x128 panel is centered in the 128x128 RAM. You **MUST** apply a column offset of **32 (0x20)**. 
    - Set Column High Nibble to `0x12` (calculated as `0x10 | (32 >> 4)`).
- **Mirror Mode Fix:** To avoid mirrored or flipped text, use the following:
    - **Segment Remap:** `0xA0` (Normal)
    - **COM Scan Direction:** `0xC8` (Flipped)
- **Vertical Resolution:** The display is physically 128 pixels tall. In Page Mode, update all **16 pages** (Page 0 to 15).

### 3. SPI Configuration
- Use **SPI3_HOST** (standard VSPI) for the internal display to avoid conflicts with the flash on SPI1.
- Firmware uses **10 MHz** clock with **SPI_DMA_CH_AUTO**.
- Clock speed of **4MHz to 10MHz** is ideal for stability.

### 4. IMU (MPU9250)
- I2C address `0x68`; WHO_AM_I register is `0x75` (value `0x71` on MPU9250, `0x38` on MPU6886).
- Firmware config: wake-up (`PWR_MGMT_1 = 0x00`), accelerometer **+/- 8g** (`ACCEL_CONFIG = 0x10`), gyroscope **+/- 2000 deg/s** (`GYRO_CONFIG = 0x18`).
- Data is read in a single burst from `ACCEL_XOUT_H (0x3B)`: accel + temperature + gyro (14 bytes) at 50Hz.
- **Die temperature conversion:** `Temp[degC] = TEMP_OUT / sens + offset`
    - MPU9250: `raw / 333.87 + 21`
    - MPU6886: `raw / 326.8 + 25`
- Yaw-rate compensation/filtering (LPF alpha, lateral/longitudinal G-sensitivity comp) is configured via `imu_config_t` in `main.c`.

### 5. ESP32 Die Temperature
- Classic ESP32 has **no temperature sensor driver** in ESP-IDF (`SOC_TEMP_SENSOR_SUPPORTED` is unset), so the ROM helper `temprature_sens_read()` (librtc.a) is used.
- Conversion: `Temp[degC] = (raw - 32) / 1.8`.
- The sensor is **known-buggy**: it frequently returns the spurious raw codes `128` (= 53.3C) and `127`. These must be filtered out (ESPHome does the same for raw `128`).
- Firmware strategy: sample at 10Hz (up to 4 quick attempts per sample), reject raw `127`/`128` and out-of-range codes, publish the **median of the last 5 valid samples**. Shown on screen mode 7; `--.- C` means no valid sample yet.

### 6. Buzzer & LED
- **LED:** GPIO 19, **active low** (write `0` to turn on).
- **Buzzer:** GPIO 26 via LEDC low-speed timer 0, 10-bit duty, 2kHz base frequency (alarm task uses 1500Hz warning / 3000Hz critical beeps).

### 7. Button A (GPIO 35)
- GPIO 35 is an **input-only** pin: no internal pull-up/pull-down is available. The firmware configures it as a plain input (`GPIO_PULLUP_DISABLE`); the board provides the bias. Active Low, falling edge = press.

## Comparison Table: Gray vs. Orange (C)
| Feature | Original M5Stick (Gray) | M5StickC (Orange) |
| :--- | :--- | :--- |
| **Display Type** | **B/W OLED (SH1107)** | Color TFT (ST7735/ST7789) |
| **PMIC** | **IP5306** | AXP192 |
| **LED Color** | **Blue (GPIO 19)** | Red (GPIO 10) |
| **Button A** | **GPIO 35** | GPIO 37 |
| **Buzzer** | GPIO 26 | GPIO 2 |
