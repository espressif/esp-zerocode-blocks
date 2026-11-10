/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
/* Host-test shim: a recursive FreeRTOS mutex is a std::recursive_mutex. */
#include "freertos/portmacro.h"
#include <mutex>

#include <atomic>
#include <chrono>
#include <thread>

typedef std::recursive_mutex *SemaphoreHandle_t;
/* For tests: how many recursive mutexes are alive, and a delay that widens
 * the window between two tasks creating one. */
inline std::atomic<int> zc_shim_live_mutexes{0};
inline int zc_shim_create_delay_us = 0;
static inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void)
{
    if (zc_shim_create_delay_us > 0) std::this_thread::sleep_for(std::chrono::microseconds(zc_shim_create_delay_us));
    zc_shim_live_mutexes++;
    return new std::recursive_mutex();
}
static inline void vSemaphoreDelete(SemaphoreHandle_t m) { zc_shim_live_mutexes--; delete m; }
static inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t m, TickType_t) { m->lock(); return pdPASS; }
static inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t m) { m->unlock(); return pdPASS; }
