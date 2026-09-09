/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Host regression test for app_driver_cb.cpp change-detection.
 *
 * The bug this guards: set_param used to apply+notify on EVERY call, so a
 * REPEATED command (same value) re-fired solution notifications — which arm
 * auto-lock / auto-off / safety-cutoff timers. A repeated "unlock" re-stamped
 * a deadbolt's auto-lock countdown and held the bolt open forever (found
 * across three product batches; the deadbolt case was a security ship-blocker).
 *
 * Compiled natively with shims (no IDF). See run.sh.
 */
#include "app_driver.h"
#include <cstdio>
#include <cstring>

/* ---- counters the fake hardware + solutions record into ---- */
static int s_apply_calls = 0;
static app_driver_param_id_t s_apply_last_id;
static app_driver_param_val_t s_apply_last_val;

/* app_driver_apply_param is normally the generated app_driver.cpp; stub it. */
esp_err_t app_driver_apply_param(app_driver_param_id_t id, app_driver_param_val_t val) {
    s_apply_calls++;
    s_apply_last_id = id;
    s_apply_last_val = val;
    return ESP_OK;
}

static int s_sol1_notify = 0;
static int s_sol2_notify = 0;
static void sol1_cb(app_driver_param_id_t, app_driver_param_val_t, app_driver_handle_t, void *) { s_sol1_notify++; }
static void sol2_cb(app_driver_param_id_t, app_driver_param_val_t, app_driver_handle_t, void *) { s_sol2_notify++; }

static int s_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok   %s\n", msg); } \
    else { printf("  FAIL %s\n", msg); s_failures++; } \
} while (0)

static app_driver_param_val_t B(bool b) { app_driver_param_val_t v; memset(&v, 0, sizeof(v)); v.b = b; return v; }
static app_driver_param_val_t U8(uint8_t u) { app_driver_param_val_t v; memset(&v, 0, sizeof(v)); v.u8 = u; return v; }

int main(void) {
    printf("app_driver set_param change-detection test\n");
    app_driver_handle_t sol1 = app_driver_register_solution("sol1", sol1_cb, nullptr);
    app_driver_handle_t sol2 = app_driver_register_solution("sol2", sol2_cb, nullptr);

    /* 1) First write of POWER=true propagates (apply + notify others). */
    app_driver_set_param(APP_DRIVER_PARAM_POWER, B(true), sol1);
    CHECK(s_apply_calls == 1, "first set(POWER=true) applies once");
    CHECK(s_sol2_notify == 1, "first set notifies the OTHER solution");
    CHECK(s_sol1_notify == 0, "source solution is not notified");

    /* 2) THE REGRESSION: repeated identical write is a no-op. */
    app_driver_set_param(APP_DRIVER_PARAM_POWER, B(true), sol1);
    app_driver_set_param(APP_DRIVER_PARAM_POWER, B(true), sol1);
    CHECK(s_apply_calls == 1, "repeated set(POWER=true) does NOT re-apply");
    CHECK(s_sol2_notify == 1, "repeated set(POWER=true) does NOT re-notify (no timer re-stamp)");

    /* 3) A genuine change still propagates. */
    app_driver_set_param(APP_DRIVER_PARAM_POWER, B(false), sol1);
    CHECK(s_apply_calls == 2, "changed value (POWER=false) applies");
    CHECK(s_sol2_notify == 2, "changed value notifies");

    /* 4) First write of a ZERO-equivalent value must still propagate
     *    (s_params is zero-initialized; the seen-flag guards this). */
    app_driver_set_param(APP_DRIVER_PARAM_LED_PATTERN, U8(0), sol1);
    CHECK(s_apply_calls == 3, "first set(LED_PATTERN=0) applies despite zero-init match");
    CHECK(s_sol2_notify == 3, "first set(LED_PATTERN=0) notifies");

    /* 5) ...and its repeat is deduped. */
    app_driver_set_param(APP_DRIVER_PARAM_LED_PATTERN, U8(0), sol1);
    CHECK(s_apply_calls == 3, "repeated set(LED_PATTERN=0) is deduped");

    /* 6) Notify still targets the right solution when sol2 is the source. */
    int before1 = s_sol1_notify;
    app_driver_set_param(APP_DRIVER_PARAM_LED_PATTERN, U8(5), sol2);
    CHECK(s_sol1_notify == before1 + 1, "sol2-sourced change notifies sol1");

    /* 7) THE EVENT PATH: fire_event propagates every time, including an
     *    identical repeat. This is what set_param must never do and what
     *    events must always do — a second doorbell press, a re-tapped preset,
     *    a media key. Real products lost every repeat to the dedup above. */
    int applyBefore = s_apply_calls;
    int notifyBefore = s_sol2_notify;
    app_driver_fire_event(APP_DRIVER_PARAM_POWER, B(true), sol1);
    app_driver_fire_event(APP_DRIVER_PARAM_POWER, B(true), sol1);
    app_driver_fire_event(APP_DRIVER_PARAM_POWER, B(true), sol1);
    CHECK(s_apply_calls == applyBefore + 3, "three identical fire_event calls apply three times");
    CHECK(s_sol2_notify == notifyBefore + 3, "three identical fire_event calls notify three times");

    /* 8) An event still skips its own source, like set_param. */
    int before1e = s_sol1_notify;
    int before2e = s_sol2_notify;
    app_driver_fire_event(APP_DRIVER_PARAM_LED_PATTERN, U8(9), sol2);
    CHECK(s_sol1_notify == before1e + 1, "sol2-sourced event notifies sol1");
    CHECK(s_sol2_notify == before2e, "sol2-sourced event does NOT notify sol2");

    /* 9) An event leaves state readable, so a later set_param of the SAME value
     *    is correctly seen as unchanged — events publish state, they don't
     *    corrupt the dedup baseline. */
    app_driver_fire_event(APP_DRIVER_PARAM_LED_PATTERN, U8(7), sol1);
    int applyAfterEvent = s_apply_calls;
    app_driver_set_param(APP_DRIVER_PARAM_LED_PATTERN, U8(7), sol1);
    CHECK(s_apply_calls == applyAfterEvent, "set_param after an identical event is still deduped");
    app_driver_param_val_t readBack;
    app_driver_get_param(APP_DRIVER_PARAM_LED_PATTERN, &readBack);
    CHECK(readBack.u8 == 7, "get_param returns the value an event published");

    /* 10) Out-of-range id is rejected, not written. */
    CHECK(app_driver_fire_event(APP_DRIVER_PARAM_MAX, B(true), sol1) == ESP_ERR_INVALID_ARG,
          "fire_event rejects an out-of-range param id");

    (void)sol1; (void)sol2;
    printf(s_failures ? "\nRESULT: FAIL (%d)\n" : "\nRESULT: PASS\n", s_failures);
    return s_failures ? 1 : 0;
}
