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
/* When set, the NEXT apply of LEVEL commits this value from "another task"
 * while the first apply is still running (simulated by re-entering set_param
 * from inside the apply). */
static bool s_race_armed = false;
static uint16_t s_race_value = 0;

esp_err_t app_driver_apply_param(app_driver_param_id_t id, app_driver_param_val_t val) {
    s_apply_calls++;
    if (s_race_armed && id == APP_DRIVER_PARAM_LEVEL) {
        s_race_armed = false;
        app_driver_param_val_t other;
        memset(&other, 0, sizeof(other));
        other.u16 = s_race_value;
        app_driver_set_param(id, other, APP_DRIVER_SOURCE_LOCAL);
    }
    /* recorded AFTER the nested write: this apply finishes last */
    s_apply_last_id = id;
    s_apply_last_val = val;
    return ESP_OK;
}

static int s_sol1_notify = 0;
static int s_sol2_notify = 0;
static void sol1_cb(app_driver_param_id_t, app_driver_param_val_t, app_driver_handle_t, void *) { s_sol1_notify++; }
static void sol2_cb(app_driver_param_id_t, app_driver_param_val_t, app_driver_handle_t, void *) { s_sol2_notify++; }

/* Records, per param, whether its last notification was an event and who sent
 * it. On a POWER event it also writes LEVEL as state, from inside the event's
 * dispatch — that nested notification must NOT read as an event. */
static bool s_rec_event[APP_DRIVER_PARAM_MAX];
static app_driver_handle_t s_rec_source[APP_DRIVER_PARAM_MAX];
static bool s_rec_nested_armed = false;
static app_driver_handle_t s_rec_handle = 0;
static void rec_cb(app_driver_param_id_t id, app_driver_param_val_t, app_driver_handle_t source, void *) {
    s_rec_event[id] = app_driver_notifying_event();
    s_rec_source[id] = source;
    if (id == APP_DRIVER_PARAM_POWER && s_rec_nested_armed) {
        s_rec_nested_armed = false;
        app_driver_param_val_t lv; memset(&lv, 0, sizeof(lv)); lv.u16 = 321;
        app_driver_set_param(APP_DRIVER_PARAM_LEVEL, lv, s_rec_handle);
        /* back in the event's own dispatch: still an event */
        s_rec_event[APP_DRIVER_PARAM_POWER] = app_driver_notifying_event();
    }
}

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

    /* 11) A notify callback can tell its sources apart by NAME — handles are
     *     assigned in registration order, which differs per product. */
    CHECK(strcmp(app_driver_solution_name(APP_DRIVER_SOURCE_LOCAL), "local") == 0,
          "solution_name(LOCAL) is \"local\"");
    CHECK(app_driver_solution_name(sol2) != NULL &&
          strcmp(app_driver_solution_name(sol2), "sol2") == 0,
          "solution_name(sol2) is the name it registered");
    CHECK(app_driver_solution_name((app_driver_handle_t)200) == NULL,
          "solution_name of an unregistered handle is NULL");

    /* 12) Two writers of one param racing: A commits and starts applying, B
     *     commits and applies inside A's apply, then A's apply finishes LAST.
     *     The hardware must still end on B, the committed value — otherwise
     *     change detection swallows the set_param(B) that would correct it. */
    app_driver_param_val_t lvA; memset(&lvA, 0, sizeof(lvA)); lvA.u16 = 100;
    s_race_armed = true;
    s_race_value = 200;
    app_driver_set_param(APP_DRIVER_PARAM_LEVEL, lvA, sol1);
    app_driver_param_val_t lvNow;
    app_driver_get_param(APP_DRIVER_PARAM_LEVEL, &lvNow);
    CHECK(lvNow.u16 == 200, "racing writers: the later commit is the stored state");
    CHECK(s_apply_last_id == APP_DRIVER_PARAM_LEVEL && s_apply_last_val.u16 == 200,
          "racing writers: the hardware ends on the stored state, not the older apply");

    /* 13) A notification knows whether it is an EVENT (fire_event) or STATE
     *     (set_param), so a "one press = one command" binding can ignore the
     *     same param written as state — a boot restore, app bookkeeping. */
    app_driver_handle_t rec = app_driver_register_solution("rec", rec_cb, nullptr);
    s_rec_handle = rec;
    CHECK(!app_driver_notifying_event(), "outside any dispatch: not an event");
    app_driver_fire_event(APP_DRIVER_PARAM_LED_PATTERN, U8(1), sol1);
    CHECK(s_rec_event[APP_DRIVER_PARAM_LED_PATTERN], "fire_event notification reads as an event");
    app_driver_set_param(APP_DRIVER_PARAM_LED_PATTERN, U8(2), sol1);
    CHECK(!s_rec_event[APP_DRIVER_PARAM_LED_PATTERN], "set_param notification does not read as an event");
    CHECK(!app_driver_notifying_event(), "after a dispatch: not an event");
    s_rec_nested_armed = true;
    app_driver_fire_event(APP_DRIVER_PARAM_POWER, B(false), sol1);
    CHECK(!s_rec_event[APP_DRIVER_PARAM_LEVEL],
          "a set_param made inside an event's dispatch is state, not the event");
    CHECK(s_rec_event[APP_DRIVER_PARAM_POWER],
          "the event's dispatch is still an event after a nested set_param");
    CHECK(!app_driver_notifying_event(), "after a nested dispatch: not an event");

    /* 14) A boot restore carries its own source: applied and announced as
     *     state, but distinguishable from a local command. */
    int applyBeforeRestore = s_apply_calls;
    app_driver_set_param(APP_DRIVER_PARAM_LED_PATTERN, U8(42), APP_DRIVER_SOURCE_RESTORE);
    CHECK(s_apply_calls == applyBeforeRestore + 1, "a restore is applied to hardware");
    CHECK(s_rec_source[APP_DRIVER_PARAM_LED_PATTERN] == APP_DRIVER_SOURCE_RESTORE,
          "a restore notifies with the RESTORE source");
    CHECK(strcmp(app_driver_solution_name(APP_DRIVER_SOURCE_RESTORE), "restore") == 0,
          "solution_name(RESTORE) is \"restore\"");

    (void)sol1; (void)sol2;
    printf(s_failures ? "\nRESULT: FAIL (%d)\n" : "\nRESULT: PASS\n", s_failures);
    return s_failures ? 1 : 0;
}
