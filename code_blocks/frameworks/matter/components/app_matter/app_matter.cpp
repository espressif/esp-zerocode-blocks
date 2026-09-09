/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Matter Protocol Solution
 *
 * BASE: No-op stub. When Matter is not used, this compiles to nothing.
 * Templates replace this file with a real Matter implementation that:
 *   1. Registers with app_driver via app_driver_register_solution()
 *   2. Creates Matter node, endpoints, clusters
 *   3. Maps Matter attribute updates to app_driver_set_param()
 *   4. Handles driver_cb to sync Matter attributes when other sources change state
 */

#include "app_matter.h"
#include <esp_log.h>

static const char *TAG = "app_matter";

esp_err_t app_matter_init(void)
{
    ESP_LOGI(TAG, "Matter solution: stub (not active)");
    return ESP_OK;
}
