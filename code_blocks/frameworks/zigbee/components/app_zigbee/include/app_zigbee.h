/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Zigbee Protocol Solution
 *
 * BASE: No-op stub. The generator replaces app_zigbee.cpp with the real
 * implementation when the product has device-type bindings.
 */

#pragma once

#include <esp_err.h>

esp_err_t app_zigbee_init(void);

/**
 * Start BDB Finding & Binding — binds this node's client clusters (remotes,
 * scene buttons) to matching targets whose F&B window is open. No-op stub
 * returns ESP_ERR_NOT_SUPPORTED; the generated implementation drives the
 * stack. Also exposed as the `zb-bind` console command.
 */
esp_err_t app_zigbee_start_finding_binding(void);
