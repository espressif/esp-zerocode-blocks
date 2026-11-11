/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
/* Host-test shim: a critical section is a real (recursive) lock, so tests that
 * write from several std::threads exercise the bus's locking for real. */
#include <mutex>
struct portMUX_TYPE {
    std::recursive_mutex m;
    portMUX_TYPE(int) {}
};
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) ((mux)->m.lock())
#define portEXIT_CRITICAL(mux)  ((mux)->m.unlock())
#define portMAX_DELAY 0xffffffffu
#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef unsigned int TickType_t;
