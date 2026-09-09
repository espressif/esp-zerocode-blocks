/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Vision Framework
 *
 * BASE: No-op stub. Templates replace with the real implementation.
 */

#include "app_vision.h"
#include <esp_log.h>

static const char *TAG = "app_vision";

esp_err_t app_vision_init(void)
{
    ESP_LOGI(TAG, "Vision framework: stub (not active)");
    return ESP_OK;
}

void zc_vision_emit(int event_id, int value)
{
    (void)event_id;
    (void)value;
}
