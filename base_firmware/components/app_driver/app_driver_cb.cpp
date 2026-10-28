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
 * during state snapshot; released before HW apply and callbacks. set_param
 * re-reads the committed value after applying, so concurrent writers of one
 * param cannot leave the hardware on the older value.
 */

#include "app_driver.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <string.h>

static const char *TAG = "app_driver_cb";

/* ──── SOLUTION REGISTRY ──────────────────────────────────────────────── */

typedef struct {
    const char *name;
    app_driver_notify_cb_t cb;
    void *ctx;
    bool active;
} solution_slot_t;

static_assert(APP_DRIVER_MAX_SOLUTIONS < APP_DRIVER_SOURCE_RESTORE,
              "solution handles must never reach the reserved restore source");

static solution_slot_t s_solutions[APP_DRIVER_MAX_SOLUTIONS];
static uint8_t s_solution_count = 0;

/* ──── PARAMETER STATE ────────────────────────────────────────────────── */

static app_driver_param_val_t s_params[APP_DRIVER_PARAM_MAX];
/* Whether each param has ever been written. s_params is zero-initialized, so
 * without this the FIRST write of a zero-equivalent value (e.g. POWER=false at
 * boot) would be deduped and never reach hardware. The first write of any param
 * always propagates; change-detection only applies from the second write on. */
static bool s_param_seen[APP_DRIVER_PARAM_MAX];
/* Sequence number of each param's last write. A registration's replay skips a
 * param written after it: that write's own notification reaches the new
 * solution. */
static uint32_t s_write_seq = 0;
static uint32_t s_param_seq[APP_DRIVER_PARAM_MAX];
/* esp_timer time of each param's last write, unchanged writes included: the
 * only liveness signal a deduped set_param leaves behind. */
static int64_t s_param_written_us[APP_DRIVER_PARAM_MAX];
/* Set by app_main before app_logic_init(): from then on a new registration is
 * handed the current value of every param already written. */
static bool s_replay_on_register = false;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

/* True while THIS task is delivering a fire_event notification. Per task, so a
 * set_param on another task — or one a callback makes from inside the event's
 * dispatch — is never mistaken for the event. */
static __thread bool s_notifying_event = false;

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
    bool replay = s_replay_on_register && cb != NULL;
    uint32_t registered_at = s_write_seq;
    portEXIT_CRITICAL(&s_lock);

    ESP_LOGI(TAG, "Registered solution '%s' (handle=%d)", name, handle);

    /* A reading taken before this registration (a driver's first poll during
     * init) is otherwise only seen at its next CHANGE: hand the current value
     * to the new solution alone, as state from RESTORE. */
    if (replay) {
        for (int i = 0; i < APP_DRIVER_PARAM_MAX; i++) {
            portENTER_CRITICAL(&s_lock);
            bool current = s_param_seen[i] && s_param_seq[i] <= registered_at;
            app_driver_param_val_t val = s_params[i];
            portEXIT_CRITICAL(&s_lock);
            if (!current) continue;
            bool outer_event = s_notifying_event;
            s_notifying_event = false;
            cb((app_driver_param_id_t)i, val, APP_DRIVER_SOURCE_RESTORE, ctx);
            s_notifying_event = outer_event;
        }
    }
    return handle;
}

void app_driver_replay_on_register(bool on)
{
    portENTER_CRITICAL(&s_lock);
    s_replay_on_register = on;
    portEXIT_CRITICAL(&s_lock);
}

const char *app_driver_solution_name(app_driver_handle_t handle)
{
    if (handle == APP_DRIVER_SOURCE_LOCAL) {
        return "local";
    }
    if (handle == APP_DRIVER_SOURCE_RESTORE) {
        return "restore";
    }
    const char *name = NULL;
    portENTER_CRITICAL(&s_lock);
    if (handle <= s_solution_count) {
        name = s_solutions[handle - 1].name;
    }
    portEXIT_CRITICAL(&s_lock);
    return name;
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

    int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_lock);
    s_param_written_us[id] = now_us;
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
    s_param_seq[id] = ++s_write_seq;
    memcpy(snapshot, s_solutions, sizeof(s_solutions));
    count = s_solution_count;
    portEXIT_CRITICAL(&s_lock);

    /* Apply to hardware (outside lock — an apply may sleep on a bus).
     *
     * Commit and apply are not one atomic step, so two tasks writing the same
     * param at once can commit A then B and APPLY B then A: the hardware ends
     * on A while the state says B, and change detection then swallows the
     * set_param(B) that would correct it. So after applying, look again: if a
     * later write committed a different value meanwhile, apply that one too.
     * Whichever task applies last re-reads after it, so the hardware ends on
     * the committed value. Bounded — a param rewritten faster than it can be
     * applied gives up after a few rounds; its own writers still apply. */
    app_driver_param_val_t applied = val;
    for (int round = 0; round < 4; round++) {
        esp_err_t err = app_driver_apply_param(id, applied);
        if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "apply_param(%d) failed: %s", id, esp_err_to_name(err));
        }
        app_driver_param_val_t current;
        portENTER_CRITICAL(&s_lock);
        current = s_params[id];
        portEXIT_CRITICAL(&s_lock);
        if (memcmp(&current, &applied, sizeof(current)) == 0) break;
        applied = current;
    }

    /* Notify all OTHER registered solutions — with the value the hardware
     * ended on, not this call's: when the loop above re-applied a later
     * writer's value, this notify may be the LAST one, and announcing `val`
     * would leave every listener (a Matter attribute, NVS) on the stale value. */
    bool outer_event = s_notifying_event;
    s_notifying_event = false;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t handle = i + 1;
        if (handle == source) continue; /* skip the source */
        if (snapshot[i].active && snapshot[i].cb) {
            snapshot[i].cb(id, applied, source, snapshot[i].ctx);
        }
    }
    s_notifying_event = outer_event;

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

    int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_lock);
    s_params[id] = val;
    s_param_seen[id] = true;
    s_param_seq[id] = ++s_write_seq;
    s_param_written_us[id] = now_us;
    memcpy(snapshot, s_solutions, sizeof(s_solutions));
    count = s_solution_count;
    portEXIT_CRITICAL(&s_lock);

    esp_err_t err = app_driver_apply_param(id, val);
    if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "apply_param(%d) failed: %s", id, esp_err_to_name(err));
    }

    bool outer_event = s_notifying_event;
    s_notifying_event = true;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t handle = i + 1;
        if (handle == source) continue;
        if (snapshot[i].active && snapshot[i].cb) {
            snapshot[i].cb(id, val, source, snapshot[i].ctx);
        }
    }
    s_notifying_event = outer_event;

    return ESP_OK;
}

bool app_driver_notifying_event(void)
{
    return s_notifying_event;
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


int64_t app_driver_param_written_us(app_driver_param_id_t id)
{
    if (id >= APP_DRIVER_PARAM_MAX) {
        return 0;
    }
    portENTER_CRITICAL(&s_lock);
    int64_t at = s_param_written_us[id];
    portEXIT_CRITICAL(&s_lock);
    return at;
}

/* ──── SERIALIZED DECISIONS ───────────────────────────────────────────── */

/* Listeners run on the writer's task, so a product decision made in one runs
 * on several tasks at once. app_driver_run_serialized() queues it for ONE task
 * instead. Why not simply hold a dispatch mutex around the listeners: the
 * Matter, Zigbee and LVGL tasks call set_param while holding their stack lock,
 * and their own listeners take that lock — a bus-wide mutex held across the
 * listeners inverts that order and deadlocks. Here nothing is held while a
 * decision runs; the table below is touched only under the spinlock. */
typedef struct {
    app_driver_serial_fn_t fn;
    void *arg;
    bool pending;
} serial_req_t;

static serial_req_t s_serial[APP_DRIVER_SERIAL_SLOTS];
static TaskHandle_t s_serial_task = NULL;
static bool s_serial_started = false;

#ifndef APP_DRIVER_SERIAL_STACK
#define APP_DRIVER_SERIAL_STACK 6144
#endif
#ifndef APP_DRIVER_SERIAL_PRIORITY
#define APP_DRIVER_SERIAL_PRIORITY 5
#endif

static void serial_task(void *unused)
{
    (void)unused;
    for (;;) {
        /* Passes over the table until a pass finds nothing waiting: each
         * request runs at most once per pass, so one asked for again and again
         * cannot starve the others. A request re-made while it runs is pending
         * again and runs on the next pass. */
        bool ran;
        do {
            ran = false;
            for (int i = 0; i < APP_DRIVER_SERIAL_SLOTS; i++) {
                app_driver_serial_fn_t fn = NULL;
                void *arg = NULL;
                portENTER_CRITICAL(&s_lock);
                if (s_serial[i].pending) {
                    s_serial[i].pending = false;
                    fn = s_serial[i].fn;
                    arg = s_serial[i].arg;
                }
                portEXIT_CRITICAL(&s_lock);
                if (fn) {
                    fn(arg);
                    ran = true;
                }
            }
        } while (ran);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

esp_err_t app_driver_run_serialized(app_driver_serial_fn_t fn, void *arg)
{
    if (fn == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int slot = -1;
    int free_slot = -1;
    portENTER_CRITICAL(&s_lock);
    for (int i = 0; i < APP_DRIVER_SERIAL_SLOTS; i++) {
        if (s_serial[i].fn == fn && s_serial[i].arg == arg) { slot = i; break; }
        if (free_slot < 0 && !s_serial[i].pending) free_slot = i;
    }
    if (slot < 0) slot = free_slot;
    if (slot >= 0) {
        s_serial[slot].fn = fn;
        s_serial[slot].arg = arg;
        s_serial[slot].pending = true;
    }
    bool start = slot >= 0 && !s_serial_started;
    if (start) s_serial_started = true;
    TaskHandle_t task = s_serial_task;
    portEXIT_CRITICAL(&s_lock);

    if (slot < 0) {
        ESP_LOGE(TAG, "run_serialized: %d different requests already waiting — not queued",
                 APP_DRIVER_SERIAL_SLOTS);
        return ESP_ERR_NO_MEM;
    }
    if (start) {
        TaskHandle_t created = NULL;
        if (xTaskCreate(serial_task, "app_serial", APP_DRIVER_SERIAL_STACK, NULL,
                        APP_DRIVER_SERIAL_PRIORITY, &created) != pdPASS) {
            portENTER_CRITICAL(&s_lock);
            s_serial_started = false;
            s_serial[slot].pending = false;
            portEXIT_CRITICAL(&s_lock);
            ESP_LOGE(TAG, "run_serialized: could not create the serial task");
            return ESP_ERR_NO_MEM;
        }
        portENTER_CRITICAL(&s_lock);
        s_serial_task = created;
        portEXIT_CRITICAL(&s_lock);
        /* A request made between the start and the store above saw no task
         * and did not notify; this wake covers it. */
        xTaskNotifyGive(created);
    } else if (task) {
        xTaskNotifyGive(task);
    }
    return ESP_OK;
}

/* ── The poll task ──────────────────────────────────────────────────────
 * One task for device polls (see app_driver_poll_every). Lower priority than
 * the serial task: a reading may wait, a decision should not. */
#ifndef APP_DRIVER_POLL_STACK
#define APP_DRIVER_POLL_STACK 4096
#endif
#ifndef APP_DRIVER_POLL_PRIORITY
#define APP_DRIVER_POLL_PRIORITY 2
#endif

typedef struct {
    app_driver_poll_fn_t fn;
    void *arg;
    int64_t period_us;
    int64_t due_us;
} poll_slot_t;

static poll_slot_t s_polls[APP_DRIVER_POLL_SLOTS];
static int s_poll_count = 0;
static bool s_poll_started = false;
static TaskHandle_t s_poll_task = NULL;

int64_t app_driver_poll_step(int64_t now_us)
{
    int64_t next = now_us + 1000000;   /* nothing due: look again in a second */
    for (int i = 0;; i++) {
        portENTER_CRITICAL(&s_lock);
        if (i >= s_poll_count) { portEXIT_CRITICAL(&s_lock); break; }
        poll_slot_t p = s_polls[i];
        portEXIT_CRITICAL(&s_lock);
        if (p.due_us <= now_us) {
            p.fn(p.arg);
            /* On schedule while it keeps up; a poll that fell behind (a slow
             * transfer, a busy CPU) restarts from now rather than bursting. */
            int64_t due = p.due_us + p.period_us;
            int64_t after = esp_timer_get_time();
            if (due <= after) due = after + p.period_us;
            portENTER_CRITICAL(&s_lock);
            s_polls[i].due_us = due;
            portEXIT_CRITICAL(&s_lock);
            p.due_us = due;
        }
        if (p.due_us < next) next = p.due_us;
    }
    return next;
}

static void poll_task(void *arg)
{
    (void)arg;
    for (;;) {
        int64_t next = app_driver_poll_step(esp_timer_get_time());
        int64_t wait_ms = (next - esp_timer_get_time() + 999) / 1000;
        TickType_t ticks = pdMS_TO_TICKS(wait_ms > 0 ? wait_ms : 0);
        /* A poll registered meanwhile is due now: the notify cuts the wait. */
        ulTaskNotifyTake(pdTRUE, ticks > 0 ? ticks : 1);
    }
}

esp_err_t app_driver_poll_every(app_driver_poll_fn_t fn, void *arg, uint32_t period_ms)
{
    if (fn == NULL || period_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&s_lock);
    if (s_poll_count >= APP_DRIVER_POLL_SLOTS) {
        portEXIT_CRITICAL(&s_lock);
        ESP_LOGE(TAG, "poll_every: %d polls already registered", APP_DRIVER_POLL_SLOTS);
        return ESP_ERR_NO_MEM;
    }
    poll_slot_t *p = &s_polls[s_poll_count];
    p->fn = fn;
    p->arg = arg;
    p->period_us = (int64_t)period_ms * 1000;
    p->due_us = 0;   /* now */
    s_poll_count++;
    bool start = !s_poll_started;
    if (start) s_poll_started = true;
    TaskHandle_t task = s_poll_task;
    portEXIT_CRITICAL(&s_lock);

#ifdef APP_DRIVER_POLL_NO_TASK   /* host tests drive app_driver_poll_step themselves */
    (void)start; (void)task; (void)poll_task;
    return ESP_OK;
#endif
    if (start) {
        TaskHandle_t created = NULL;
        if (xTaskCreate(poll_task, "app_poll", APP_DRIVER_POLL_STACK, NULL,
                        APP_DRIVER_POLL_PRIORITY, &created) != pdPASS) {
            portENTER_CRITICAL(&s_lock);
            s_poll_started = false;
            portEXIT_CRITICAL(&s_lock);
            ESP_LOGE(TAG, "poll_every: could not create the poll task");
            return ESP_ERR_NO_MEM;
        }
        portENTER_CRITICAL(&s_lock);
        s_poll_task = created;
        portEXIT_CRITICAL(&s_lock);
        xTaskNotifyGive(created);
    } else if (task) {
        xTaskNotifyGive(task);
    }
    return ESP_OK;
}
