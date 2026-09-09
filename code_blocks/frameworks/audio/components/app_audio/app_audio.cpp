/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Voice (esp-sr) Framework
 *
 * BASE: No-op stub. Templates replace with the real esp-sr implementation.
 */

#include "app_audio.h"
#include <esp_log.h>

static const char *TAG = "app_audio";

esp_err_t app_audio_init(void)
{
    ESP_LOGI(TAG, "Voice framework: stub (not active)");
    return ESP_OK;
}
