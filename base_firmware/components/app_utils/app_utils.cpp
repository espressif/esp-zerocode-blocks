/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Utility Functions
 */

#include "app_utils.h"

#include <esp_log.h>
#include <esp_system.h>
#include <esp_heap_caps.h>

void app_utils_print_mem(const char *label)
{
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);

    ESP_LOGI("MEM", "[%s] free=%u min=%u largest=%u",
             label ? label : "?",
             (unsigned)free_heap,
             (unsigned)min_free,
             (unsigned)largest);
}
