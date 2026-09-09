/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - ESP-NOW Framework
 *
 * BASE: No-op stub. Templates replace with the real implementation.
 */

#include "app_espnow.h"
#include <esp_log.h>

static const char *TAG = "app_espnow";

esp_err_t app_espnow_init(void)
{
    ESP_LOGI(TAG, "ESP-NOW framework: stub (not active)");
    return ESP_OK;
}

void zc_espnow_send(const char *param_name, uint8_t type, uint32_t value)
{
    (void)param_name; (void)type; (void)value;
}

void zc_espnow_pair_start(uint32_t window_ms) { (void)window_ms; }
bool zc_espnow_pairing(void) { return false; }
uint8_t zc_espnow_peer_count(void) { return 0; }
void zc_espnow_forget_peers(void) {}
