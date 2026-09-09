/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Driver Callback Infrastructure
 *
 * Manages the solution registry, parameter state, and notification dispatch.
 * This file is NEVER template-overridden — it provides the coordination
 * layer that all protocol solutions depend on.
 *
 * Thread safety: Uses portMUX_TYPE spinlock (ISR-safe). Lock held only
 * during state snapshot; released before HW apply and callbacks.
 */

#include "app_driver.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <string.h>

static const char *TAG = "app_driver_cb";

/* ──── SOLUTION REGISTRY ──────────────────────────────────────────────── */

typedef struct {
    const char *name;
    app_driver_notify_cb_t cb;
    void *ctx;
    bool active;
} solution_slot_t;

static solution_slot_t s_solutions[APP_DRIVER_MAX_SOLUTIONS];
static uint8_t s_solution_count = 0;

/* ──── PARAMETER STATE ────────────────────────────────────────────────── */

static app_driver_param_val_t s_params[APP_DRIVER_PARAM_MAX];
/* Whether each param has ever been written. s_params is zero-initialized, so
 * without this the FIRST write of a zero-equivalent value (e.g. POWER=false at
 * boot) would be deduped and never reach hardware. The first write of any param
 * always propagates; change-detection only applies from the second write on. */
static bool s_param_seen[APP_DRIVER_PARAM_MAX];
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

/* ──── PUBLIC API ─────────────────────────────────────────────────────── */

app_driver_handle_t app_driver_register_solution(
    const char *name,
    app_driver_notify_cb_t cb,
    void *ctx)
{
    portENTER_CRITICAL(&s_lock);
    if (s_solution_count >= APP_DRIVER_MAX_SOLUTIONS) {
        portEXIT_CRITICAL(&s_lock);
        ESP_LOGE(TAG, "Cannot register solution '%s': registry full", name);
        return 0;
    }
    uint8_t handle = s_solution_count + 1; /* handles are 1-based, 0 = LOCAL */
    s_solutions[s_solution_count].name = name;
    s_solutions[s_solution_count].cb = cb;
    s_solutions[s_solution_count].ctx = ctx;
    s_solutions[s_solution_count].active = true;
    s_solution_count++;
    portEXIT_CRITICAL(&s_lock);

    ESP_LOGI(TAG, "Registered solution '%s' (handle=%d)", name, handle);
    return handle;
}

esp_err_t app_driver_set_param(
    app_driver_param_id_t id,
    app_driver_param_val_t val,
    app_driver_handle_t source)
{
    if (id >= APP_DRIVER_PARAM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Change detection: a write that doesn't change the value is a no-op —
     * no hardware re-apply, no notifications. This is what makes set_param
     * idempotent, which matters because solutions ARM behavior on the notify
     * (auto-lock/auto-off/safety timers, edge handlers). Without this, a
     * redundant command — a controller re-asserting desired state, a user
     * re-tapping the same button, an overlapping schedule — re-fires those
     * timers. A repeated "unlock" would re-stamp a deadbolt's auto-lock
     * countdown and hold the bolt open indefinitely; repeated "on" would
     * re-stamp a heater's hard-cutoff timer. Deduping here fixes the whole
     * class at the source, and spares NVS the redundant writes.
     *
     * memcmp is safe for the value union: any real value change alters the
     * active member's bytes, so a genuine change is never dropped (memcmp is
     * non-zero and we fall through). The only thing it can miss is a no-op it
     * fails to recognize (e.g. indeterminate padding), which merely reverts to
     * the old always-apply behavior — never unsafe. Callers that ever need a
     * forced re-apply of the same value should drive the hardware directly. */
    solution_slot_t snapshot[APP_DRIVER_MAX_SOLUTIONS];
    uint8_t count;

    portENTER_CRITICAL(&s_lock);
    bool unchanged = s_param_seen[id] && (memcmp(&s_params[id], &val, sizeof(val)) == 0);
    if (unchanged) {
        portEXIT_CRITICAL(&s_lock);
        /* A dropped write used to be completely silent — ESP_OK, no log, nothing
         * on the wire. That is what made the "event routed through a state
         * channel" bug invisible: a second button press, a re-tapped preset and a
         * manual override that matched the current value all vanished here, and
         * no test looking at a device log could see it happen. Anything that must
         * propagate every time is an EVENT — use app_driver_fire_event(). */
        ESP_LOGD(TAG, "set_param(%d) unchanged — not propagated (events must use fire_event)", id);
        return ESP_OK;
    }
    s_params[id] = val;
    s_param_seen[id] = true;
    memcpy(snapshot, s_solutions, sizeof(s_solutions));
    count = s_solution_count;
    portEXIT_CRITICAL(&s_lock);

    /* Apply to hardware (outside lock) */
    esp_err_t err = app_driver_apply_param(id, val);
    if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "apply_param(%d) failed: %s", id, esp_err_to_name(err));
    }

    /* Notify all OTHER registered solutions */
    for (uint8_t i = 0; i < count; i++) {
        uint8_t handle = i + 1;
        if (handle == source) continue; /* skip the source */
        if (snapshot[i].active && snapshot[i].cb) {
            snapshot[i].cb(id, val, source, snapshot[i].ctx);
        }
    }

    return ESP_OK;
}

esp_err_t app_driver_fire_event(
    app_driver_param_id_t id,
    app_driver_param_val_t val,
    app_driver_handle_t source)
{
    if (id >= APP_DRIVER_PARAM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    /* The EVENT path: something happened, and it happened again. No change
     * detection — every call applies and notifies, even with an identical value.
     *
     * set_param() models STATE ("the lamp is on"), where a redundant write must
     * be a no-op or safety timers re-arm. An event is the opposite: a doorbell
     * press, an alarm firing, a timer expiring and a treat button all carry
     * "again" as their entire meaning, and deduplicating them by value drops
     * every occurrence after the first. Routing events through set_param broke
     * real products — a chime that rang once per boot, a manual override the
     * light ignored, a media button that produced nothing on the second press.
     * Use this for anything whose repeat matters; use set_param for state. */
    solution_slot_t snapshot[APP_DRIVER_MAX_SOLUTIONS];
    uint8_t count;

    portENTER_CRITICAL(&s_lock);
    s_params[id] = val;
    s_param_seen[id] = true;
    memcpy(snapshot, s_solutions, sizeof(s_solutions));
    count = s_solution_count;
    portEXIT_CRITICAL(&s_lock);

    esp_err_t err = app_driver_apply_param(id, val);
    if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "apply_param(%d) failed: %s", id, esp_err_to_name(err));
    }

    for (uint8_t i = 0; i < count; i++) {
        uint8_t handle = i + 1;
        if (handle == source) continue;
        if (snapshot[i].active && snapshot[i].cb) {
            snapshot[i].cb(id, val, source, snapshot[i].ctx);
        }
    }

    return ESP_OK;
}

bool app_driver_param_seen(app_driver_param_id_t id)
{
    if (id >= APP_DRIVER_PARAM_MAX) {
        return false;
    }
    portENTER_CRITICAL(&s_lock);
    bool seen = s_param_seen[id];
    portEXIT_CRITICAL(&s_lock);
    return seen;
}

esp_err_t app_driver_get_param(
    app_driver_param_id_t id,
    app_driver_param_val_t *val)
{
    if (id >= APP_DRIVER_PARAM_MAX || !val) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&s_lock);
    *val = s_params[id];
    portEXIT_CRITICAL(&s_lock);

    return ESP_OK;
}

