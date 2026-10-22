/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Utility Functions
 *
 * Lightweight utilities with no app-level dependencies.
 * Safe to include from ANY component without circular dependency risk.
 */

#pragma once

#include <stdint.h>
#include <esp_err.h>
#include <driver/ledc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Print current heap memory stats with a label.
 * Call at strategic points for memory analysis during testing.
 *
 * Output format: MEM [label]: free=XXXXX min=XXXXX largest=XXXXX
 */
void app_utils_print_mem(const char *label);

/**
 * Configure an LEDC timer at the HIGHEST duty resolution its clock reaches for
 * cfg->freq_hz, probing from the chip's widest (SOC_LEDC_TIMER_BIT_WIDTH) down.
 * A fixed resolution is not portable: 13-bit at 20 kHz needs a ~164 MHz clock,
 * which an ESP32-H2 does not have.
 *
 * cfg->duty_resolution is ignored on input and holds the chosen resolution on
 * return. Logs the choice. When no resolution works for the frequency it logs
 * that and returns ESP_ERR_NOT_SUPPORTED — it never aborts.
 *
 * @param cfg       timer config (speed_mode, timer_num, freq_hz, clk_cfg set)
 * @param max_duty  out: full-scale duty at the chosen resolution, 2^bits - 1
 *                  (0 on failure)
 */
esp_err_t app_utils_ledc_timer_config(ledc_timer_config_t *cfg, uint32_t *max_duty);

#ifdef __cplusplus
}
#endif
