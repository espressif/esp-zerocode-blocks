/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - BLE HID Framework
 *
 * BASE: No-op stub. Templates replace with the real implementation.
 */

#include "app_ble_hid.h"
#include <esp_log.h>

static const char *TAG = "app_ble_hid";

esp_err_t app_ble_hid_init(void)
{
    ESP_LOGI(TAG, "BLE HID framework: stub (not active)");
    return ESP_OK;
}

void zc_ble_hid_send_consumer(uint16_t usage)
{
    (void)usage;
}
