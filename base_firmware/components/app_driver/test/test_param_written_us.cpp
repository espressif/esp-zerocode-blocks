/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Host test: app_driver_param_written_us() moves on EVERY write — a set_param
 * that changes nothing (and so notifies nobody) included — so product code can
 * tell "the sensor is alive and reads the same" from "the sensor stopped".
 * Compiled natively with shims (see run.sh).
 */
#include "app_driver.h"
#include <esp_timer.h>
#include <cstdio>
#include <cstring>

esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }

static int s_notified = 0;
static void cb(app_driver_param_id_t, app_driver_param_val_t, app_driver_handle_t, void *) { s_notified++; }

static int s_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok   %s\n", msg); } \
    else { printf("  FAIL %s\n", msg); s_failures++; } \
} while (0)

static app_driver_param_val_t U16(uint16_t u) { app_driver_param_val_t v; memset(&v, 0, sizeof(v)); v.u16 = u; return v; }

int main(void) {
    printf("app_driver param_written_us test\n");
    app_driver_register_solution("listener", cb, NULL);

    CHECK(app_driver_param_written_us(APP_DRIVER_PARAM_LEVEL) == 0, "a param never written reads 0");
    CHECK(app_driver_param_written_us(APP_DRIVER_PARAM_MAX) == 0, "an out-of-range id reads 0");

    zc_test_now_us = 1000;
    app_driver_set_param(APP_DRIVER_PARAM_LEVEL, U16(1200), APP_DRIVER_SOURCE_LOCAL);
    CHECK(app_driver_param_written_us(APP_DRIVER_PARAM_LEVEL) == 1000, "a first write stamps the time");
    CHECK(s_notified == 1, "...and notifies");

    zc_test_now_us = 16000000;
    app_driver_set_param(APP_DRIVER_PARAM_LEVEL, U16(1200), APP_DRIVER_SOURCE_LOCAL);
    CHECK(s_notified == 1, "an unchanged write is still not notified");
    CHECK(app_driver_param_written_us(APP_DRIVER_PARAM_LEVEL) == 16000000,
          "an unchanged write still moves the written time");

    zc_test_now_us = 17000000;
    app_driver_fire_event(APP_DRIVER_PARAM_LEVEL, U16(1200), APP_DRIVER_SOURCE_LOCAL);
    CHECK(app_driver_param_written_us(APP_DRIVER_PARAM_LEVEL) == 17000000, "an event moves it too");

    CHECK(app_driver_param_written_us(APP_DRIVER_PARAM_POWER) == 0, "another param is untouched");

    printf(s_failures ? "\nRESULT: FAIL (%d)\n" : "\nRESULT: PASS\n", s_failures);
    return s_failures ? 1 : 0;
}
