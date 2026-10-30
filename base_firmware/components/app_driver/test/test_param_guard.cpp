/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Host regression test for app_driver_set_guard(): a guarded write is checked
 * BEFORE it is committed or applied, so an interlock (an E-stop, a lock's hour
 * rule, "never heat mid-cook") keeps the hardware from switching at all —
 * listener-side refusals let the relay energise first and only undid it after.
 * A refused write leaves the param as it was, tells only its source the real
 * value (so a controller's tile goes back), and a clamped write applies the
 * clamped value. Compiled natively with shims (no IDF). See run.sh.
 */
#include "app_driver.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

static int s_power_applies_on = 0;   /* how often the relay was driven ON */
static int s_level_last = -1;
esp_err_t app_driver_apply_param(app_driver_param_id_t id, app_driver_param_val_t val) {
    if (id == APP_DRIVER_PARAM_POWER && val.b) s_power_applies_on++;
    if (id == APP_DRIVER_PARAM_LEVEL) s_level_last = val.u16;
    return ESP_OK;
}

static std::atomic<bool> s_estop{false};
static bool power_guard(app_driver_param_id_t, app_driver_param_val_t *v, app_driver_handle_t, void *) {
    return !(v->b && s_estop.load());
}
static bool level_guard(app_driver_param_id_t, app_driver_param_val_t *v, app_driver_handle_t, void *) {
    if (v->u16 > 100) v->u16 = 100;   /* clamp, not refuse */
    return true;
}

static int s_matter_told = 0, s_matter_told_val = -1, s_other_told = 0;
static void matter_cb(app_driver_param_id_t id, app_driver_param_val_t v, app_driver_handle_t, void *) {
    if (id == APP_DRIVER_PARAM_POWER) { s_matter_told++; s_matter_told_val = v.b; }
}
static void other_cb(app_driver_param_id_t id, app_driver_param_val_t, app_driver_handle_t, void *) {
    if (id == APP_DRIVER_PARAM_POWER) s_other_told++;
}

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main()
{
    app_driver_handle_t matter = app_driver_register_solution("matter", matter_cb, nullptr);
    app_driver_register_solution("other", other_cb, nullptr);
    CHECK(app_driver_set_guard(APP_DRIVER_PARAM_POWER, power_guard, nullptr) == ESP_OK);
    CHECK(app_driver_set_guard(APP_DRIVER_PARAM_POWER, power_guard, nullptr) == ESP_ERR_INVALID_STATE);
    CHECK(app_driver_set_guard(APP_DRIVER_PARAM_LEVEL, level_guard, nullptr) == ESP_OK);

    app_driver_param_val_t off = {}, on = {};
    on.b = true;
    CHECK(app_driver_set_param(APP_DRIVER_PARAM_POWER, off, APP_DRIVER_SOURCE_LOCAL) == ESP_OK);
    s_estop = true;
    s_matter_told = 0; s_other_told = 0;

    /* Matter asks for ON during the E-stop: refused before the relay moves. */
    CHECK(app_driver_set_param(APP_DRIVER_PARAM_POWER, on, matter) == ESP_ERR_NOT_ALLOWED);
    CHECK(s_power_applies_on == 0);
    app_driver_param_val_t now = {};
    app_driver_get_param(APP_DRIVER_PARAM_POWER, &now);
    CHECK(now.b == false);
    CHECK(s_matter_told == 1 && s_matter_told_val == 0);   /* its tile goes back to off */
    CHECK(s_other_told == 0);                               /* nothing changed for anyone else */
    CHECK(app_driver_fire_event(APP_DRIVER_PARAM_POWER, on, APP_DRIVER_SOURCE_LOCAL) == ESP_ERR_NOT_ALLOWED);
    CHECK(s_power_applies_on == 0);

    /* Released: the same write goes through. */
    s_estop = false;
    CHECK(app_driver_set_param(APP_DRIVER_PARAM_POWER, on, matter) == ESP_OK);
    CHECK(s_power_applies_on == 1);

    /* A clamping guard: 250 is applied and stored as 100. */
    app_driver_param_val_t big = {};
    big.u16 = 250;
    CHECK(app_driver_set_param(APP_DRIVER_PARAM_LEVEL, big, APP_DRIVER_SOURCE_LOCAL) == ESP_OK);
    app_driver_get_param(APP_DRIVER_PARAM_LEVEL, &now);
    CHECK(s_level_last == 100 && now.u16 == 100);

    /* The race: an ON whose check passed cannot land after the E-stop's OFF.
     * Many rounds of "engage E-stop + write OFF" against "write ON": whenever
     * both are done and the E-stop is engaged, the relay state must be OFF. */
    int bad = 0;
    for (int r = 0; r < 2000; r++) {
        s_estop = false;
        app_driver_set_param(APP_DRIVER_PARAM_POWER, off, APP_DRIVER_SOURCE_LOCAL);
        std::thread a([&] { app_driver_set_param(APP_DRIVER_PARAM_POWER, on, matter); });
        std::thread b([&] { s_estop = true; app_driver_set_param(APP_DRIVER_PARAM_POWER, off, APP_DRIVER_SOURCE_LOCAL); });
        a.join(); b.join();
        app_driver_get_param(APP_DRIVER_PARAM_POWER, &now);
        if (now.b) bad++;
    }
    CHECK(bad == 0);

    printf("%s param_guard (%d bad races)\n", fails ? "FAILED" : "ok", bad);
    return fails ? 1 : 0;
}
