/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Host regression test: the guard lock is created once, however many tasks
 * put their first guards on at the same moment. Two locks would let two
 * guarded writes check and apply at once. Compiled natively with shims (no
 * IDF). See run.sh.
 */
#include "app_driver.h"
#include "freertos/semphr.h"
#include <atomic>
#include <cstdio>
#include <thread>

esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }

static bool pass(app_driver_param_id_t, app_driver_param_val_t *, app_driver_handle_t, void *) { return true; }

int main()
{
    zc_shim_create_delay_us = 2000;
    std::atomic<bool> go{false};
    std::thread t[3];
    const app_driver_param_id_t ids[3] = { APP_DRIVER_PARAM_POWER, APP_DRIVER_PARAM_LED_PATTERN, APP_DRIVER_PARAM_LEVEL };
    esp_err_t res[3] = {};
    for (int i = 0; i < 3; i++) {
        t[i] = std::thread([&, i] {
            while (!go.load()) {}
            res[i] = app_driver_set_guard(ids[i], pass, nullptr);
        });
    }
    go = true;
    for (auto &th : t) th.join();
    int fails = 0;
    for (int i = 0; i < 3; i++) if (res[i] != ESP_OK) { printf("FAIL set_guard %d -> %d\n", i, (int)res[i]); fails++; }
    if (zc_shim_live_mutexes.load() != 1) { printf("FAIL %d guard locks alive, want 1\n", zc_shim_live_mutexes.load()); fails++; }
    if (fails) return 1;
    printf("test_guard_lock_once: ok\n");
    return 0;
}
