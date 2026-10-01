#include "cpu_temp.h"
#include <string.h>
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "soc/soc.h"
#include "soc/sens_reg.h"

// The classic ESP32 has no temperature sensor driver in ESP-IDF
// (SOC_TEMP_SENSOR_SUPPORTED is not set for this target), but the ROM
// helper temprature_sens_read() (librtc.a) performs the full conversion
// and returns an 8-bit raw code. The hardware is known to return spurious
// codes (128 => 53.3C is the classic one, filtered by ESPHome too), so
// every sample is validated against known-bad codes and the physically
// plausible raw range.
extern uint8_t temprature_sens_read(void);

#define TSENS_ATTEMPTS_PER_CALL 6
#define TSENS_RETRY_DELAY_US    3000
#define TSENS_SETTLE_US         5000
#define TSENS_RAW_MIN           40    //  3.9 C
#define TSENS_RAW_MAX           230   // 104.4 C

static const char *TAG = "CPU_TEMP";

static int s_total;
static int s_valid;

// Keep the sensor powered with the ROM's clock divider (CLK_DIV=2) so
// conversions are already running when temprature_sens_read() samples it.
// The ROM helper clobbers these bits on every call, so this must be
// re-applied after each read - without it, nearly every sample is 128.
static void tsens_prepare(void)
{
    uint32_t ctrl = REG_READ(SENS_SAR_TSENS_CTRL_REG);
    ctrl = (ctrl & ~SENS_TSENS_CLK_DIV_M) | (2 << SENS_TSENS_CLK_DIV_S);
    ctrl |= SENS_TSENS_POWER_UP_FORCE | SENS_TSENS_POWER_UP;
    REG_WRITE(SENS_SAR_TSENS_CTRL_REG, ctrl);
}

static inline bool raw_is_valid(uint8_t raw)
{
    // 127/128 are the stuck/garbage codes seen on this silicon;
    // everything else must be in the plausible die-temperature range.
    if (raw == 127 || raw == 128) {
        return false;
    }
    return raw >= TSENS_RAW_MIN && raw <= TSENS_RAW_MAX;
}

static inline float raw_to_celsius(uint8_t raw)
{
    return ((float)raw - 32.0f) / 1.8f;
}

esp_err_t cpu_temp_init(void)
{
    s_total = 0;
    s_valid = 0;
    tsens_prepare();
    esp_rom_delay_us(TSENS_SETTLE_US);
    ESP_LOGI(TAG, "using ROM temprature_sens_read() (no IDF driver for classic ESP32)");
    return ESP_OK;
}

esp_err_t cpu_temp_get(float *out_celsius, cpu_temp_dbg_t *dbg)
{
    cpu_temp_dbg_t local;
    if (!dbg) dbg = &local;
    memset(dbg, 0, sizeof(*dbg));

    if (!out_celsius) return ESP_ERR_INVALID_ARG;

    for (int a = 1; a <= TSENS_ATTEMPTS_PER_CALL; a++) {
        uint8_t raw = temprature_sens_read();
        tsens_prepare(); // restore divider/power clobbered by the ROM helper
        dbg->attempts = a;
        dbg->raw = raw;
        s_total++;
        if (raw_is_valid(raw)) {
            s_valid++;
            dbg->valid_total = s_valid;
            dbg->total = s_total;
            *out_celsius = raw_to_celsius(raw);
            return ESP_OK;
        }
        esp_rom_delay_us(TSENS_RETRY_DELAY_US);
    }

    dbg->valid_total = s_valid;
    dbg->total = s_total;
    return ESP_ERR_INVALID_STATE;
}
