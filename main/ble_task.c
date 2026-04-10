#include "ble_task.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "common_state.h"

static const char *tag = "BLE_TASK";
static uint8_t ble_addr_type;

static void ble_app_advertise(void);

static int ble_app_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(tag, "Advertising complete");
            ble_app_advertise();
            break;
        default:
            break;
    }
    return 0;
}

static void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)"M5Stick-Gray";
    fields.name_len = strlen("M5Stick-Gray");
    fields.name_is_complete = 1;

    // Get current accel data for manufacturer data
    mpu6886_data_t local_accel;
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10))) {
        local_accel = g_state.accel;
        xSemaphoreGive(g_state_mutex);
    } else {
        memset(&local_accel, 0, sizeof(local_accel));
    }

    // Pack accel data into mfg data (6 bytes: 2 for X, 2 for Y, 2 for Z as int16 scaled by 1000)
    uint8_t mfg_data[8];
    mfg_data[0] = 0xFF; // Company ID Lo (Example)
    mfg_data[1] = 0xFF; // Company ID Hi
    int16_t ax = (int16_t)(local_accel.x * 1000);
    int16_t ay = (int16_t)(local_accel.y * 1000);
    int16_t az = (int16_t)(local_accel.z * 1000);
    memcpy(&mfg_data[2], &ax, 2);
    memcpy(&mfg_data[4], &ay, 2);
    memcpy(&mfg_data[6], &az, 2);

    fields.mfg_data = mfg_data;
    fields.mfg_data_len = sizeof(mfg_data);

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(tag, "Error setting advertisement data; rc=%d", rc);
        return;
    }

    if (ble_gap_adv_active()) {
        return; // Already advertising with new fields
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_app_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(tag, "Error starting advertisement; rc=%d", rc);
        return;
    }
}

static void ble_app_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    assert(rc == 0);
    ble_app_advertise();
}

static void ble_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_task(void *pvParameters)
{
    // Initialize NVS (required for BLE)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(nimble_port_init());

    ble_hs_cfg.sync_cb = ble_app_on_sync;

    nimble_port_freertos_init(ble_host_task);

    // Periodically update advertising data with new sensor values
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (ble_hs_synced()) {
            ble_app_advertise(); // Update mfg data
        }
    }
}
