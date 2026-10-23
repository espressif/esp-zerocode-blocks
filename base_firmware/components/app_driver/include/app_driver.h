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
#include "app_driver_types.h"

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
 * @return ESP_OK on success
 */
esp_err_t app_driver_set_param(
    app_driver_param_id_t id,
    app_driver_param_val_t val,
    app_driver_handle_t source);

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
