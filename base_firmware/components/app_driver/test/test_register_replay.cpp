/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Host test: a solution registered after app_driver_replay_on_register(true)
 * is handed the current value of every param already written, once, and no
 * other solution hears it again. Compiled natively with shims (see run.sh).
 */
#include "app_driver.h"
#include <cstdio>
#include <cstring>

static int s_apply_calls = 0;
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) {
    s_apply_calls++;
    return ESP_OK;
}

struct rec_t {
    int calls[APP_DRIVER_PARAM_MAX];
    app_driver_param_val_t last[APP_DRIVER_PARAM_MAX];
    app_driver_handle_t source[APP_DRIVER_PARAM_MAX];
    bool event[APP_DRIVER_PARAM_MAX];
};
static rec_t s_early, s_late, s_plain;

static void rec_cb(app_driver_param_id_t id, app_driver_param_val_t v, app_driver_handle_t src, void *ctx) {
    rec_t *r = (rec_t *)ctx;
    r->calls[id]++;
    r->last[id] = v;
    r->source[id] = src;
    r->event[id] = app_driver_notifying_event();
}

/* Writes LEVEL from inside its replay of POWER: that write must reach the
 * listener once, through the normal path, and not again through the replay. */
static rec_t s_writer;
static void writer_cb(app_driver_param_id_t id, app_driver_param_val_t v, app_driver_handle_t src, void *ctx) {
    rec_cb(id, v, src, ctx);
    if (id == APP_DRIVER_PARAM_POWER && src == APP_DRIVER_SOURCE_RESTORE) {
        app_driver_param_val_t lv; memset(&lv, 0, sizeof(lv)); lv.u16 = 77;
        app_driver_set_param(APP_DRIVER_PARAM_LEVEL, lv, APP_DRIVER_SOURCE_LOCAL);
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
    printf("app_driver replay-on-register test\n");

    /* Boot order: a framework registers, a driver takes its first reading. */
    app_driver_handle_t early = app_driver_register_solution("early", rec_cb, &s_early);
    app_driver_set_param(APP_DRIVER_PARAM_POWER, B(true), APP_DRIVER_SOURCE_LOCAL);
    app_driver_set_param(APP_DRIVER_PARAM_LED_PATTERN, U8(0), APP_DRIVER_SOURCE_LOCAL);
    CHECK(s_early.calls[APP_DRIVER_PARAM_POWER] == 1, "early solution hears the first reading");

    /* Without the switch, a late registration hears nothing until a change. */
    app_driver_register_solution("plain", rec_cb, &s_plain);
    int plain_calls = 0;
    for (int i = 0; i < APP_DRIVER_PARAM_MAX; i++) plain_calls += s_plain.calls[i];
    CHECK(plain_calls == 0, "replay off: a registration is not handed current values");

    /* app_main turns it on before app_logic_init(). */
    app_driver_replay_on_register(true);
    int applies = s_apply_calls;
    app_driver_handle_t late = app_driver_register_solution("late", rec_cb, &s_late);
    CHECK(late != 0, "late registration succeeds");
    CHECK(s_late.calls[APP_DRIVER_PARAM_POWER] == 1 && s_late.last[APP_DRIVER_PARAM_POWER].b,
          "late solution is handed POWER=true once");
    CHECK(s_late.calls[APP_DRIVER_PARAM_LED_PATTERN] == 1 && s_late.last[APP_DRIVER_PARAM_LED_PATTERN].u8 == 0,
          "late solution is handed a zero reading (written, so a value)");
    CHECK(s_late.calls[APP_DRIVER_PARAM_LEVEL] == 0, "a param never written is not handed over");
    CHECK(s_late.source[APP_DRIVER_PARAM_POWER] == APP_DRIVER_SOURCE_RESTORE,
          "the hand-over carries the RESTORE source");
    CHECK(!s_late.event[APP_DRIVER_PARAM_POWER], "the hand-over is state, not an event");
    CHECK(s_apply_calls == applies, "the hand-over does not re-apply hardware");
    CHECK(s_early.calls[APP_DRIVER_PARAM_POWER] == 1 && s_plain.calls[APP_DRIVER_PARAM_POWER] == 0,
          "other solutions are not notified again");

    /* Dedup is unchanged for everyone. */
    app_driver_set_param(APP_DRIVER_PARAM_POWER, B(true), APP_DRIVER_SOURCE_LOCAL);
    CHECK(s_late.calls[APP_DRIVER_PARAM_POWER] == 1 && s_early.calls[APP_DRIVER_PARAM_POWER] == 1,
          "an identical later write is still deduped");
    app_driver_set_param(APP_DRIVER_PARAM_POWER, B(false), early);
    CHECK(s_late.calls[APP_DRIVER_PARAM_POWER] == 2 && !s_late.last[APP_DRIVER_PARAM_POWER].b,
          "a change reaches the late solution as usual");
    CHECK(s_early.calls[APP_DRIVER_PARAM_POWER] == 1, "the source is still skipped");

    /* A write made during the hand-over is delivered once, by that write. */
    app_driver_register_solution("writer", writer_cb, &s_writer);
    CHECK(s_writer.calls[APP_DRIVER_PARAM_LEVEL] == 1 && s_writer.last[APP_DRIVER_PARAM_LEVEL].u16 == 77,
          "a param written during the hand-over reaches the new solution once");
    CHECK(s_writer.source[APP_DRIVER_PARAM_LEVEL] == APP_DRIVER_SOURCE_LOCAL,
          "...through the write, not the replay");

    (void)late;
    printf(s_failures ? "\nRESULT: FAIL (%d)\n" : "\nRESULT: PASS\n", s_failures);
    return s_failures ? 1 : 0;
}
