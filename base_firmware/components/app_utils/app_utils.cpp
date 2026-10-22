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
#include <soc/soc_caps.h>

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

esp_err_t app_utils_ledc_timer_config(ledc_timer_config_t *cfg, uint32_t *max_duty)
{
    if (!cfg || !max_duty || cfg->freq_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    /* The LEDC driver logs an error for every resolution it cannot reach.
       Those are expected while probing, so its tag is silenced and restored. */
    const esp_log_level_t ledc_level = esp_log_level_get("ledc");
    esp_log_level_set("ledc", ESP_LOG_NONE);
    esp_err_t err = ESP_FAIL;
    int bits = SOC_LEDC_TIMER_BIT_WIDTH;
    for (; bits >= 1; --bits) {
        cfg->duty_resolution = (ledc_timer_bit_t)bits;
        err = ledc_timer_config(cfg);
        if (err == ESP_OK) {
            break;
        }
    }
    esp_log_level_set("ledc", ledc_level);
    if (err != ESP_OK) {
        *max_duty = 0;
        ESP_LOGE("app_utils", "LEDC timer %d: %u Hz cannot be generated at any duty resolution on this chip",
                 (int)cfg->timer_num, (unsigned)cfg->freq_hz);
        return ESP_ERR_NOT_SUPPORTED;
    }
    *max_duty = (1UL << bits) - 1U;
    ESP_LOGI("app_utils", "LEDC timer %d: %u Hz at %d-bit duty resolution",
             (int)cfg->timer_num, (unsigned)cfg->freq_hz, bits);
    return ESP_OK;
}
