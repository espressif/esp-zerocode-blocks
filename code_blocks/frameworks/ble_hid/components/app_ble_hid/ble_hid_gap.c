/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * BLE-only GAP helper for the BLE HID framework — NimBLE host.
 *
 * Adapted (reduced to the BLE/NimBLE path) from ESP-IDF's
 * examples/bluetooth/esp_hid_device/main/esp_hid_gap.c (Apache-2.0,
 * Espressif Systems). Changes: BLE-only, Just Works pairing (a remote has
 * no display for a passkey), automatic re-advertising on disconnect.
 *
 * Compiled only in generated trees with device-type bindings — the stub
 * CMakeLists does not list this file.
 */

#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"

#if CONFIG_BT_NIMBLE_ENABLED

#include "esp_log.h"
#include "esp_hidd.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "ble_hid_gap.h"

static const char *TAG = "ble_hid_gap";

#define GATT_SVR_SVC_HID_UUID 0x1812

static struct ble_hs_adv_fields s_fields;
static const char *s_device_name = "ZeroCode Remote";

void ble_store_config_init(void);

esp_err_t ble_hid_gap_adv_init(uint16_t appearance, const char *device_name)
{
    static ble_uuid16_t hid_uuid = BLE_UUID16_INIT(GATT_SVR_SVC_HID_UUID);

    s_device_name = device_name;
    memset(&s_fields, 0, sizeof(s_fields));
    s_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    s_fields.appearance = appearance;
    s_fields.appearance_is_present = 1;
    s_fields.tx_pwr_lvl_is_present = 1;
    s_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    s_fields.name = (const uint8_t *)device_name;
    s_fields.name_len = strlen(device_name);
    s_fields.name_is_complete = 1;
    s_fields.uuids16 = &hid_uuid;
    s_fields.num_uuids16 = 1;
    s_fields.uuids16_is_complete = 1;

    /* Just Works: no IO on a remote — bonded + secure connections, no
     * passkey ceremony. */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    return ESP_OK;
}

static int ble_hid_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        if (event->connect.status != 0) {
            ble_hid_gap_adv_start();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected (reason=%d) — advertising again", event->disconnect.reason);
        ble_hid_gap_adv_start();
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        /* 180 s window elapsed with no taker — keep the device joinable. */
        ble_hid_gap_adv_start();
        return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* Re-pairing from a bonded peer: drop the stale bond and accept. */
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    default:
        return 0;
    }
}

esp_err_t ble_hid_gap_adv_start(void)
{
    struct ble_gap_adv_params adv_params;
    int rc = ble_gap_adv_set_fields(&s_fields);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "adv_set_fields rc=%d", rc);
        return ESP_FAIL;
    }
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, 180000 /* ms */,
                           &adv_params, ble_hid_gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "adv_start rc=%d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void ble_hid_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_hid_gap_start_host(void)
{
    ble_store_config_init();
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    return esp_nimble_enable(ble_hid_host_task);
}

esp_err_t ble_hid_gap_stack_init(void)
{
    return nimble_port_init();
}

#endif /* CONFIG_BT_NIMBLE_ENABLED */
