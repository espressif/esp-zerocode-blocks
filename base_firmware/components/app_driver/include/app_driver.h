/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Hardware Driver Interface
 *
 * The driver owns hardware state and a callback registry.
 * Protocol solutions (Matter, RainMaker, Zigbee, BLE Mesh) register
 * with the driver and are notified when state changes from any source.
 *
 * ──── ARCHITECTURE ─────────────────────────────────────────────────
 * - app_driver.cpp: Hardware-specific init + apply_param() dispatch
 *   (template-overridden per device type)
 * - app_driver_cb.cpp: Callback registry, set_param(), get_param()
 *   (NEVER template-overridden)
 * - app_driver_types.h: Param enum, value union, callback types
 *   (NEVER template-overridden)
 *
 * FLOW:
 *   Solution calls set_param(POWER, {.b=true}, handle)
 *     → app_driver_cb.cpp updates state under lock
 *     → calls app_driver_apply_param() (in app_driver.cpp) to drive HW
 *     → notifies all OTHER registered solutions
 * ─────────────────────────────────────────────────────────────────
 */

#pragma once

#include <esp_err.h>
#include <stdint.h>
#include "app_driver_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Polls app_driver_poll_every() can hold. */
#ifndef APP_DRIVER_POLL_SLOTS
#define APP_DRIVER_POLL_SLOTS 24
#endif

/** Distinct app_driver_run_serialized() requests that can wait at once. */
#ifndef APP_DRIVER_SERIAL_SLOTS
#define APP_DRIVER_SERIAL_SLOTS 16
#endif

/**
 * Source for a write that RESTORES remembered state at boot (an NVS persist
 * behavior putting back the last power/level). Use it as the `source` of
 * app_driver_set_param() for such writes — never APP_DRIVER_SOURCE_LOCAL.
 *
 * A restore is local state, not a command: a solution that turns a param
 * change into a message to OTHER devices (a mesh/Zigbee/ESP-NOW controller
 * binding) must ignore notifications whose source is this, or every reboot
 * switches the room. Hardware is still applied and every solution is still
 * notified, so state holders (a Matter attribute, a display) stay in step.
 * No registered solution ever has this handle (nor has Zigbee's resync source,
 * 0xFF).
 */
#define APP_DRIVER_SOURCE_RESTORE ((app_driver_handle_t)0xFE)

/**
 * Initialize hardware peripherals.
 * Template-overridden per device type.
 */
esp_err_t app_driver_init(void);

/**
 * Apply a parameter change to hardware.
 * Called by the callback infrastructure (app_driver_cb.cpp).
 * Implemented in app_driver.cpp (template-overridden per device type).
 *
 * @param id   Parameter to apply
 * @param val  New value
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if param not handled
 */
esp_err_t app_driver_apply_param(app_driver_param_id_t id, app_driver_param_val_t val);

/**
 * Register a protocol solution with the driver.
 * Returns a handle used to identify this solution in set_param() calls.
 *
 * Registered from app_logic_init() on, the callback is also called at once
 * with the current value of every param already written — see
 * app_driver_replay_on_register().
 *
 * @param name  Human-readable name (e.g., "matter", "rainmaker")
 * @param cb    Callback invoked when OTHER sources change parameters
 * @param ctx   User context passed to callback
 * @return Handle for this solution, or 0 on failure
 */
app_driver_handle_t app_driver_register_solution(
    const char *name,
    app_driver_notify_cb_t cb,
    void *ctx);

/**
 * Whether a solution registered from now on is first handed the CURRENT value
 * of every param already written, once, through its own callback (source
 * APP_DRIVER_SOURCE_RESTORE, not an event). Other solutions are not notified.
 *
 * app_main turns it on just before app_logic_init(): drivers and behaviors
 * take their first reading during init, and a set_param of the same value
 * later is deduped, so a product listener registered after that reading would
 * otherwise not see the param until it changes. Frameworks and behaviors
 * register earlier and keep the plain behaviour.
 *
 * @param on  true to replay current values to each new registration
 */
void app_driver_replay_on_register(bool on);

/**
 * Name a solution was registered under, for a handle a notify callback was
 * given as `source`. Returns "local" for APP_DRIVER_SOURCE_LOCAL, "restore" for
 * APP_DRIVER_SOURCE_RESTORE, and NULL for a
 * handle nobody registered.
 *
 * Handles are assigned in registration order, which differs per product (the
 * display framework even registers from its own setup task), so a callback
 * that must tell one solution from another — a touch on the device's OWN
 * screen versus a command from across the network — can only do it by name.
 *
 * @param handle  A source handle
 * @return The registered name, "local", "restore", or NULL
 */
const char *app_driver_solution_name(app_driver_handle_t handle);

/**
 * Set a parameter STATE. Updates state, applies to hardware, and notifies all
 * registered solutions EXCEPT the source.
 *
 * Change-detected: writing the value it already holds is a no-op — no hardware
 * re-apply, no notifications. That is deliberate (a repeated "unlock" must not
 * re-stamp a deadbolt's auto-lock countdown), and it makes this the WRONG call
 * for anything whose repetition is the point. For those, see
 * app_driver_fire_event().
 *
 * @param id      Parameter to change
 * @param val     New value
 * @param source  Handle of the calling solution (or APP_DRIVER_SOURCE_LOCAL)
 * @return ESP_OK on success; ESP_ERR_NOT_ALLOWED when the param's guard
 *         refused it (see app_driver_set_guard)
 */
esp_err_t app_driver_set_param(
    app_driver_param_id_t id,
    app_driver_param_val_t val,
    app_driver_handle_t source);

/**
 * A guard on one param: called with the value a write is about to commit,
 * BEFORE it is committed or applied to the hardware. Return false to refuse
 * the write; change *val to clamp it. It may read other params
 * (app_driver_get_param) but must not block for long.
 */
typedef bool (*app_driver_guard_fn_t)(app_driver_param_id_t id, app_driver_param_val_t *val,
                                      app_driver_handle_t source, void *ctx);

/**
 * Put a guard on a param: the one place an interlock belongs (an E-stop that
 * keeps a pump off, a lock's allowed hours, "never heat mid-cook", a range).
 *
 * A listener cannot refuse a write — the hardware has already switched when
 * it is told, and setting the param back only undoes it afterwards (a relay
 * energised for a moment, an app tile that flips and flips back). A guard runs
 * first: a refused write (set_param or fire_event) returns ESP_ERR_NOT_ALLOWED,
 * the param and the hardware stay as they were, and only the write's SOURCE
 * solution is told the current value, so a controller's tile goes back while
 * no other listener sees anything happen.
 *
 * The check, the commit and the hardware apply of a guarded param run under
 * one bus lock, so a write the guard passed can never land after a later
 * write that the interlock's own state change caused (set the E-stop flag,
 * then write the pump off: an "on" checked before the flag lands first).
 * One guard per param; register it from app_logic_init(), before anything
 * writes the param.
 *
 * That lock is held while the guard runs and while the param is applied to
 * the hardware. Neither may WAIT for another task that writes a guarded param
 * (a queue it fills, a mutex it holds across app_driver_set_param): that task
 * waits for the lock, and both stop. Writing a param directly from the guard
 * or the apply, on the same task, is fine.
 *
 * @return ESP_OK, ESP_ERR_INVALID_ARG (bad id or NULL fn), ESP_ERR_INVALID_STATE
 *         (the param already has a guard), ESP_ERR_NO_MEM
 */
esp_err_t app_driver_set_guard(app_driver_param_id_t id, app_driver_guard_fn_t fn, void *ctx);

/**
 * Whether a write made on THIS task was refused by its param's guard since the
 * last call; *id (may be NULL) gets the param. Clears the record. Framework
 * handlers call it after their cases: a case that writes a command and then
 * echoes the commanded value to its controller (a RainMaker tile, an MQTT
 * state topic) would otherwise show a refused command as done, so the handler
 * re-reports the param's real value after it (Matter instead rejects the
 * write). Product code rarely needs it: set_param's return says the same.
 */
bool app_driver_take_refusal(app_driver_param_id_t *id);

/**
 * Fire an EVENT on a parameter. Always applies and always notifies, even when
 * the value is identical to the last one.
 *
 * Use this whenever "it happened again" is the meaning: a button or doorbell
 * press, an alarm ringing, a timer expiring, a treat dispensed, a motion
 * re-trigger, a media key. Use app_driver_set_param() for state a controller
 * asserts ("on", "50 %", "locked").
 *
 * Rule of thumb: if two identical calls in a row should produce two effects,
 * this is the call you want.
 *
 * @param id      Parameter carrying the event
 * @param val     Value to publish (may equal the current value)
 * @param source  Handle of the calling solution (or APP_DRIVER_SOURCE_LOCAL)
 * @return ESP_OK on success
 */
esp_err_t app_driver_fire_event(
    app_driver_param_id_t id,
    app_driver_param_val_t val,
    app_driver_handle_t source);

/**
 * Whether the notification being delivered right now — on the calling task —
 * comes from app_driver_fire_event() (an EVENT: a press, a ring) rather than
 * app_driver_set_param() (STATE: a value written, restored or synced).
 *
 * Call it from inside an app_driver_notify_cb_t. A binding whose param means
 * "one press = one command" (a scene/switch button sending Toggle) must act
 * only when this is true: the same param written as state — a restore at
 * boot, an app keeping its own bookkeeping in it — is not a press.
 *
 * @return true while delivering a fire_event notification on this task
 */
bool app_driver_notifying_event(void);

/**
 * Get the current value of a parameter.
 *
 * @param id   Parameter to query
 * @param val  Output: current value
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if id out of range
 */
esp_err_t app_driver_get_param(
    app_driver_param_id_t id,
    app_driver_param_val_t *val);

/**
 * Whether a parameter has been written at least once (by set_param or
 * fire_event) since boot. Before that, app_driver_get_param() returns the
 * zero-initialized union — a value, but not a reading. A measurement's first
 * publish, a "no data" indicator, a decision that must not run on a default:
 * all gate on this.
 *
 * @param id  Parameter to check
 * @return true once the param has carried a real value
 */
bool app_driver_param_seen(app_driver_param_id_t id);

/**
 * esp_timer time (µs since boot) of the last write of a parameter — EVERY
 * app_driver_set_param(), including one that changed nothing (which is not
 * notified), and every app_driver_fire_event(). 0 if never written.
 *
 * An unchanged write is not notified, so a listener cannot tell "the sensor is
 * alive and reads the same value" from "the sensor stopped" by value changes.
 * Ask this instead (now - written > a few poll intervals = stale), or wire the
 * driver's status param, which says so directly. Cheap: one read under the
 * bus lock.
 *
 * @param id  Parameter to check
 * @return esp_timer_get_time() of its last write, or 0
 */
int64_t app_driver_param_written_us(app_driver_param_id_t id);

/** A product decision run by app_driver_run_serialized(). */
typedef void (*app_driver_serial_fn_t)(void *arg);

/**
 * Run fn(arg) on the bus's ONE serial task, never at the same time as another
 * request made through this call.
 *
 * Listeners run on whichever task wrote the param — a sensor poll, MQTT, the
 * web server, the console, Matter — so a decision made inside a listener
 * (read mode + target + temperature, write the relay) runs on several tasks at
 * once and an older decision can land last. Make the listener ask for the
 * decision instead, and let the decision read every param it needs itself:
 *
 *   static void decide(void *arg) { ...get_param x3..., set_param(RELAY) }
 *   static void cb(id, val, src, ctx) { if (id is an input) app_driver_run_serialized(decide, NULL); }
 *
 * Requests run in turn on one task. A request for the same fn + arg that is
 * still waiting is merged into it (the decision re-reads the params when it
 * runs, so one run covers every write before it); one made while that
 * decision is running — from its own set_param's listeners, or from inside it
 * — runs again after it. Never hold a mutex of your own across
 * app_driver_set_param() instead: a listener on the Matter, Zigbee or LVGL
 * task that waits for it while you wait for that task's lock deadlocks both.
 * Not callable from an ISR. The task is created on the first call.
 *
 * @param fn   Decision to run (not NULL)
 * @param arg  Passed to fn; part of the identity for merging
 * @return ESP_OK, ESP_ERR_INVALID_ARG for a NULL fn, ESP_ERR_NO_MEM when
 *         APP_DRIVER_SERIAL_SLOTS different requests are already waiting
 */
esp_err_t app_driver_run_serialized(app_driver_serial_fn_t fn, void *arg);

/** A device poll run by app_driver_poll_every(). */
typedef void (*app_driver_poll_fn_t)(void *arg);

/**
 * Run fn(arg) now and then every period_ms on the bus's ONE poll task.
 *
 * For a block that reads a device: an I2C/SPI/1-Wire transfer, a conversion it
 * has to wait for, a re-probe of a part that is missing. Those waits are fine
 * here and nowhere near the shared esp_timer task, where a 10-30 ms wait or a
 * 50 ms bus timeout stalled every other timer in the product. Polls run one at
 * a time, in turn; a slow poll delays the next ones, never skips them. Not
 * callable from an ISR. The task is created on the first call.
 *
 * @return ESP_OK, ESP_ERR_INVALID_ARG for a NULL fn or a 0 period,
 *         ESP_ERR_NO_MEM when APP_DRIVER_POLL_SLOTS polls are registered
 */
esp_err_t app_driver_poll_every(app_driver_poll_fn_t fn, void *arg, uint32_t period_ms);

/**
 * One pass of the poll task: run every poll due at now_us, return when the
 * next one is due (esp_timer_get_time() time base). Exposed for host tests.
 */
int64_t app_driver_poll_step(int64_t now_us);

#ifdef __cplusplus
}
#endif
