/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Zigbee Protocol Solution
 *
 * BASE: No-op stub. Templates replace with real Zigbee implementation.
 */

#include "app_zigbee.h"
#include <esp_log.h>

static const char *TAG = "app_zigbee";

esp_err_t app_zigbee_init(void)
{
    ESP_LOGI(TAG, "Zigbee solution: stub (not active)");
    return ESP_OK;
}

esp_err_t app_zigbee_start_finding_binding(void)
{
    ESP_LOGW(TAG, "Finding & Binding: stub (not active)");
    return ESP_ERR_NOT_SUPPORTED;
}
