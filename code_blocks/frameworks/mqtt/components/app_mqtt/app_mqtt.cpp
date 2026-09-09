/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - MQTT (Home Assistant) Framework
 *
 * BASE: No-op stub. Templates replace with the real esp-mqtt implementation.
 */

#include "app_mqtt.h"
#include <esp_log.h>

static const char *TAG = "app_mqtt";

esp_err_t app_mqtt_init(void)
{
    ESP_LOGI(TAG, "MQTT framework: stub (not active)");
    return ESP_OK;
}
