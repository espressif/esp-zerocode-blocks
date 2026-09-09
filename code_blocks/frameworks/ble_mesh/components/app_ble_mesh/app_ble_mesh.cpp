/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - BLE Mesh Protocol Solution
 *
 * BASE: No-op stub. The generator replaces this file with the real
 * implementation for every product that selects the framework. It defines
 * every symbol app_ble_mesh.h declares so that a product which somehow gets
 * here still LINKS — a missing symbol at link time says nothing about what
 * went wrong, a log line does.
 */

#include "app_ble_mesh.h"
#include <esp_log.h>

static const char *TAG = "app_ble_mesh";

esp_err_t app_ble_mesh_init(void)
{
    ESP_LOGI(TAG, "BLE Mesh solution: stub (not active)");
    return ESP_OK;
}

bool zc_ble_mesh_provisioned(void)
{
    return false;
}

esp_err_t zc_ble_mesh_join_mode(void)
{
    ESP_LOGW(TAG, "join: stub (not active)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t zc_ble_mesh_factory_reset(void)
{
    ESP_LOGW(TAG, "reset: stub (not active)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t zc_ble_mesh_publish_onoff(uint16_t addr, bool on)
{
    (void)addr;
    (void)on;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t zc_ble_mesh_publish_level(uint16_t addr, int16_t level)
{
    (void)addr;
    (void)level;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t zc_ble_mesh_set_onoff_state(bool on)
{
    (void)on;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t zc_ble_mesh_set_level_state(int16_t level)
{
    (void)level;
    return ESP_ERR_NOT_SUPPORTED;
}

int16_t zc_ble_mesh_level_from_u8(uint8_t value)
{
    return (int16_t)((int32_t)value * 65535 / 254 - 32768);
}

uint8_t zc_ble_mesh_level_to_u8(int16_t level)
{
    return (uint8_t)(((int32_t)level + 32768) * 254 / 65535);
}
