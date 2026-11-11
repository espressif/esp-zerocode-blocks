/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// CXXFLAGS: -DAPP_DRIVER_POLL_NO_TASK

/**
 * Host test: app_driver_poll_every() runs each poll at once and then on its
 * own period, a slow poll delays but never skips the others, a poll that fell
 * behind restarts from now instead of bursting, and bad registrations are
 * refused. Drives app_driver_poll_step() by hand on the shim clock.
 */
#include "app_driver.h"
#include "esp_timer.h"
#include <cstdio>
#include <string>

esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }

static std::string g_trace;
static void fast(void *) { g_trace += "f"; }
static void slow(void *) { g_trace += "s"; zc_test_now_us += 30000; }   /* takes 30 ms */

static int fails = 0;
#define EXPECT(cond, what) do { if (!(cond)) { printf("FAIL: %s\n", what); fails++; } } while (0)

int main()
{
    EXPECT(app_driver_poll_every(NULL, NULL, 10) == ESP_ERR_INVALID_ARG, "NULL fn refused");
    EXPECT(app_driver_poll_every(fast, NULL, 0) == ESP_ERR_INVALID_ARG, "0 period refused");
    zc_test_now_us = 1000000;
    EXPECT(app_driver_poll_every(fast, NULL, 100) == ESP_OK, "fast registered");
    EXPECT(app_driver_poll_every(slow, NULL, 1000) == ESP_OK, "slow registered");

    int64_t next = app_driver_poll_step(zc_test_now_us);           /* both due now */
    EXPECT(g_trace == "fs", "both run at once");
    EXPECT(next == 1000000 + 100000, "next is the fast one, on its schedule");

    g_trace.clear();
    for (int i = 0; i < 11; i++) { zc_test_now_us = next; next = app_driver_poll_step(zc_test_now_us); }
    /* fast every 100 ms (1.1 .. 2.0 s): 10 runs; slow once more, 1 s after its
       first run ENDED (it took 30 ms): at 2.03 s, the 11th step */
    int f = 0, s = 0;
    for (char c : g_trace) { if (c == 'f') f++; else s++; }
    if (!(f == 10 && s == 1)) printf("trace %s\n", g_trace.c_str());
    EXPECT(f == 10 && s == 1, "fast on its period, slow once more after a second");

    g_trace.clear();
    zc_test_now_us += 5000000;                                      /* 5 s with no step */
    app_driver_poll_step(zc_test_now_us);
    app_driver_poll_step(zc_test_now_us);
    EXPECT(g_trace == "fs", "fell behind: each runs once, no burst of missed runs");

    for (int i = 0; i < APP_DRIVER_POLL_SLOTS; i++) app_driver_poll_every(fast, NULL, 100);
    EXPECT(app_driver_poll_every(fast, NULL, 100) == ESP_ERR_NO_MEM, "a full table is refused");

    if (fails) return 1;
    printf("test_poll_every: ok\n");
    return 0;
}
