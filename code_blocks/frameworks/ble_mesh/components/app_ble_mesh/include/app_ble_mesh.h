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

/** As zc_ble_mesh_publish_onoff, for Generic Level Set Unacknowledged. */
esp_err_t zc_ble_mesh_publish_level(uint16_t addr, int16_t level);

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
