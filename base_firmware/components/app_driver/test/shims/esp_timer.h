/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
/* Host-test shim: the clock is a variable the test sets. */
#include <cstdint>
inline int64_t zc_test_now_us = 0;
static inline int64_t esp_timer_get_time(void) { return zc_test_now_us; }
