/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
/* Host-test shim: a recursive FreeRTOS mutex is a std::recursive_mutex. */
#include "freertos/portmacro.h"
#include <mutex>

typedef std::recursive_mutex *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) { return new std::recursive_mutex(); }
static inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t m, TickType_t) { m->lock(); return pdPASS; }
static inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t m) { m->unlock(); return pdPASS; }
