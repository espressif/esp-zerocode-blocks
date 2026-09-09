/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Business Logic Interface
 *
 * Handles automation rules, state machines, power-on behavior,
 * transitions, timers, and any product-specific logic.
 *
 * The AI implements this based on the product's requirements.
 */

#pragma once

#include <esp_err.h>

/**
 * Initialize the composed behaviors (console commands, persistence, factory
 * reset, diagnostics, …). GENERATED into zc_behaviors.cpp and called by
 * app_main before app_logic_init(); not a place for product logic.
 */
esp_err_t zc_behaviors_init(void);

/**
 * Initialize business logic (state machines, timers, automation rules).
 * The product's own entry point — app_logic.cpp.
 */
esp_err_t app_logic_init(void);
