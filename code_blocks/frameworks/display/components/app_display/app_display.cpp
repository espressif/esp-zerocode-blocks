/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Display (LVGL) Framework
 *
 * BASE: No-op stub. Templates replace with the real LVGL implementation.
 */

#include "app_display.h"
#include <esp_log.h>

static const char *TAG = "app_display";

esp_err_t app_display_init(void)
{
    ESP_LOGI(TAG, "Display framework: stub (not active)");
    return ESP_OK;
}
