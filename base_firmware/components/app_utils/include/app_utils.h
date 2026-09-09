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

#include <esp_err.h>

/**
 * Print current heap memory stats with a label.
 * Call at strategic points for memory analysis during testing.
 *
 * Output format: MEM [label]: free=XXXXX min=XXXXX largest=XXXXX
 */
void app_utils_print_mem(const char *label);
