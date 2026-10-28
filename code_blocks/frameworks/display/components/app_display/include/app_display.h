/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Display (LVGL) Framework
 *
 * BASE: No-op stub. The generator replaces app_display.cpp with the real
 * implementation when the product has device-type bindings.
 */

#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_display_init(void);

/**
 * Called on EVERY touch of the screen, wherever it lands — before any object
 * sees it, so a full-screen object cannot swallow it. Define it in product
 * code to wake a dimmed backlight or switch views. Runs on the LVGL task with
 * the LVGL lock held: call lv_* directly, never lvgl_port_lock, never block.
 * The default does nothing.
 */
void app_display_on_touch(void);

#ifdef __cplusplus
}
#endif
