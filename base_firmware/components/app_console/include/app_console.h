/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Device Console Interface
 *
 * Provides an interactive CLI over the serial port with built-in
 * diagnostic commands (reboot, factory_reset, mem) and a registration
 * API for product-specific commands added by the AI.
 *
 * Uses ESP-IDF's esp_console component (UART REPL).
 */

#pragma once

#include <esp_err.h>
#include <esp_console.h>

/**
 * Initialize the console REPL and register built-in commands.
 * Call this AFTER esp_matter::start() so Matter is fully initialized.
 */
esp_err_t app_console_init(void);

/**
 * Register a custom console command.
 * Product-specific commands are registered from app_driver or app_logic.
 */
esp_err_t app_console_register_cmd(const esp_console_cmd_t *cmd);
