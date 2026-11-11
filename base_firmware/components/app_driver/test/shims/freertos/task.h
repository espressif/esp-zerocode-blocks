/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
/* Host-test shim: a task is a detached std::thread; a task notification is a
 * counter under a condition variable. Enough for the bus's serial task. */
#include "freertos/portmacro.h"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

struct zc_test_task {
    std::mutex m;
    std::condition_variable cv;
    uint32_t count = 0;
};
typedef zc_test_task *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

inline thread_local zc_test_task *zc_test_current_task = nullptr;

static inline BaseType_t xTaskCreate(TaskFunction_t fn, const char *, uint32_t, void *arg,
                                     UBaseType_t, TaskHandle_t *out)
{
    zc_test_task *t = new zc_test_task();
    if (out) *out = t;
    std::thread([fn, arg, t]() { zc_test_current_task = t; fn(arg); }).detach();
    return pdPASS;
}

static inline BaseType_t xTaskNotifyGive(TaskHandle_t t)
{
    { std::lock_guard<std::mutex> g(t->m); t->count++; }
    t->cv.notify_one();
    return pdPASS;
}

static inline uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t)
{
    zc_test_task *t = zc_test_current_task;
    std::unique_lock<std::mutex> g(t->m);
    t->cv.wait(g, [t] { return t->count > 0; });
    uint32_t n = t->count;
    t->count = clear ? 0 : n - 1;
    return n;
}

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#endif
