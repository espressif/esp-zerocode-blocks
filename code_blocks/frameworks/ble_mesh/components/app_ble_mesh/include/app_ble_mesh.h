/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - BLE Mesh Protocol Solution
 *
 * BASE: No-op stub. The generator replaces app_ble_mesh.cpp with the real
 * implementation; this header is the contract both honour, so a device-type
 * or behavior slot compiles the same way either side of that replacement.
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * C linkage for everything below, the hooks included: the framework is C++,
 * a product's code may be C or C++, and a hook defined with the other linkage
 * than the framework's weak default is a DIFFERENT symbol — it compiles, links
 * and is never called.
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Default group address. Every Generic server on a generated node is
 * subscribed to it and every client publishes to it, so a controller and an
 * actuator provisioned into the same network work together without either
 * product knowing the other's unicast address.
 */
#define ZC_MESH_GROUP_ADDR 0xC000

/**
 * The first two bytes of a generated node's device UUID: a fixed marker a
 * provisioner matches on, so it provisions the kit's own devices and ignores
 * every other unprovisioned beacon in range. The remaining bytes carry the
 * device's BLE address, which keeps each UUID unique.
 */
#define ZC_MESH_UUID_B0 0x5A /* 'Z' */
#define ZC_MESH_UUID_B1 0x43 /* 'C' */

/** Company identifier the composition data reports. */
#define ZC_MESH_CID 0x02E5

esp_err_t app_ble_mesh_init(void);

/**
 * Is this node provisioned? Reads the MESH STACK's own answer
 * (esp_ble_mesh_node_is_provisioned), which is restored from settings on
 * boot — never a private flag set only when a provisioning event arrives,
 * which is how a node ends up advertising "joining" forever after a reboot
 * it already survived.
 */
bool zc_ble_mesh_provisioned(void);

/**
 * Advertise as unprovisioned (PB-ADV + PB-GATT) so a provisioner or a phone
 * can pick this node up. Refused while the node is already provisioned —
 * factory-reset it first.
 */
esp_err_t zc_ble_mesh_join_mode(void);

/**
 * Leave the network: reset the node and erase its stored mesh settings, then
 * start advertising as unprovisioned again. Recoverable WITHOUT a reboot —
 * the node is joinable the moment this returns.
 */
esp_err_t zc_ble_mesh_factory_reset(void);

/**
 * Publish a Generic OnOff Set Unacknowledged through this node's OnOff CLIENT
 * model. `addr` is a unicast or group address (ZC_MESH_GROUP_ADDR being the
 * usual one). Carries a fresh transaction id, so two identical commands in a
 * row are two commands rather than a discarded retransmission.
 *
 * Refused with ESP_ERR_INVALID_STATE (and a log line saying why) while the
 * client model has no application key bound to it — which is the state a node
 * is in until its provisioner binds one.
 */
esp_err_t zc_ble_mesh_publish_onoff(uint16_t addr, bool on);

/**
 * TOGGLE the lights at `addr`. Generic OnOff has NO toggle value: a Set
 * carries an absolute 0 or 1, so a switch that always sends 1 (or always 0, or
 * alternates on a private flag) is not a toggle. This publishes a Generic
 * OnOff Set Unacknowledged carrying the INVERSE of the last-known state:
 *
 *   1. the most recent Generic OnOff Status this node's OnOff client heard
 *      (any light publishing its status, its target state when it reports a
 *      transition), else
 *   2. the value this node last published (zc_ble_mesh_publish_onoff or a
 *      previous toggle), else
 *   3. off — so the first toggle after boot turns the lights ON.
 *
 * Whichever of 1 and 2 happened LAST wins. One remembered state for the node,
 * not per address: a switch drives one room. The framework subscribes its
 * clients to the group they publish to, so the Status a light publishes there
 * reaches it (a ZeroCode provisioner points every light's servers at that
 * group; a phone app must configure the lights' status publication to it).
 */
esp_err_t zc_ble_mesh_publish_toggle(uint16_t addr);

/** As zc_ble_mesh_publish_onoff, for Generic Level Set Unacknowledged. */
esp_err_t zc_ble_mesh_publish_level(uint16_t addr, int16_t level);

/**
 * DIM / BRIGHTEN relative to where the lights ARE: a Generic Delta Set
 * Unacknowledged through the Level client. Never compute an absolute level
 * from this switch's own guess and send it with zc_ble_mesh_publish_level —
 * that snaps the whole room to the guess.
 *
 * `delta` is THIS call's step, in Generic Level units: the level is an int16
 * (-32768 = off/minimum .. 32767 = full), so the whole range is 65535 and a
 * 10% step is about 6553 (negative dims, positive brightens). Lights clamp at
 * the ends.
 *
 * `continue_hold` says whether this call belongs to the gesture already in
 * progress. `false` starts a new gesture (a new transaction id). `true`
 * continues it — call it every tick of a long press with the same step and the
 * lights move smoothly from where they were when the press began:
 *
 *   - to a UNICAST address the framework does what the Mesh Model spec
 *     defines: it re-sends the RUNNING TOTAL of the gesture's steps under the
 *     gesture's TID, which the light applies to its level at the start of the
 *     transaction (a lost message is made up by the next one);
 *   - to a GROUP address every step is sent as its own transaction (fresh
 *     TID, just the step). ESP-IDF's Generic Level server only recognises a
 *     transaction on a unicast destination, so a running total under one TID
 *     sent to a group would be ADDED to the current level on every message
 *     and accelerate; a fresh step per message moves the same distance on
 *     every server.
 *
 * A gesture also ends by itself 5 s after its last message (the spec's 6 s
 * transaction window, with margin), so a `true` after a pause starts afresh.
 */
esp_err_t zc_ble_mesh_publish_delta(uint16_t addr, int32_t delta, bool continue_hold);

/**
 * Update this node's own Generic OnOff SERVER state and publish the resulting
 * status. This is what a device type calls when its hardware changed for a
 * local reason (a button, a sensor, a schedule) so the mesh sees it.
 */
esp_err_t zc_ble_mesh_set_onoff_state(bool on);

/** As zc_ble_mesh_set_onoff_state, for the Generic Level server. */
esp_err_t zc_ble_mesh_set_level_state(int16_t level);

/**
 * The one definition of how a 0-254 driver level (what brightness, position
 * and volume params carry) maps onto the mesh's signed 16-bit Generic Level,
 * used in both directions so a round trip cannot drift.
 */
int16_t zc_ble_mesh_level_from_u8(uint8_t value);
uint8_t zc_ble_mesh_level_to_u8(int16_t level);

/*
 * ── Hooks ────────────────────────────────────────────────────────────
 * The framework OWNS every mesh callback registration. A product must NOT
 * call esp_ble_mesh_register_prov_callback / _generic_client_callback /
 * _generic_server_callback / _config_server_callback itself: there is one slot
 * per callback, so the framework's registration in app_ble_mesh_init()
 * replaces the product's (dead callback) or the product's replaces the
 * framework's (provisioning, status tracking and the device-type bindings
 * stop). Define these instead.
 *
 * Each has a WEAK do-nothing default in the framework; define a function with
 * exactly this signature in any C or C++ file that includes this header (the
 * declarations here give it C linkage) and yours replaces the default. Put it
 * in a file that is linked anyway — components/app_logic (linked whole) or the
 * same file as code something already calls: a static library member that
 * nothing references is never pulled in, and then the default runs.
 *
 * They run on the mesh stack's task: keep them short, never block, and hand
 * real work to the driver bus (app_driver_set_param / app_driver_fire_event)
 * or a task of your own.
 */

/** A Generic OnOff Status reached this node's OnOff client: `src` is the
 *  light's unicast address, `on` its state (its target when it reports a
 *  transition). Called after the framework updated the state
 *  zc_ble_mesh_publish_toggle reads. */
void zc_ble_mesh_on_onoff_status(uint16_t src, bool on);

/** As zc_ble_mesh_on_onoff_status, for a Generic Level Status (`level` in
 *  Generic Level units, see zc_ble_mesh_publish_delta). */
void zc_ble_mesh_on_level_status(uint16_t src, int16_t level);

/** Provisioning changed: `true` when this node is added to a network, and
 *  ONCE at init when it boots already provisioned (restored from settings);
 *  `false` when it leaves (zc_ble_mesh_factory_reset or a provisioner's node
 *  reset). Not called on the device that IS the provisioner — it forms the
 *  network rather than joining one. */
void zc_ble_mesh_on_provisioned(bool provisioned);

#ifdef __cplusplus
}
#endif
