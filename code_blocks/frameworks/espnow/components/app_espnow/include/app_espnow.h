/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - ESP-NOW (device-to-device) Framework
 *
 * BASE: No-op stub. The generator replaces app_espnow.cpp with the real
 * implementation when the product has device-type bindings.
 */

#pragma once

#include <esp_err.h>
#include <stdint.h>

esp_err_t app_espnow_init(void);

#include <stdbool.h>

/** Send a param change to peers (name is the kit vocabulary — hashed on the
 *  wire). Broadcast while the device holds no peers; unicast to each paired
 *  peer once it does. type: 0 bool, 1 u8, 2 i16, 3 u16, 4 u32. */
void zc_espnow_send(const char *param_name, uint8_t type, uint32_t value);

/** Open the pairing window for window_ms (0 = 30 s). Open it on BOTH devices:
 *  each stores the other and from then on they talk only to their peers.
 *  Stored in NVS. Bench: `espnow-pair`, `espnow-peers`, `espnow-forget`. */
void zc_espnow_pair_start(uint32_t window_ms);
bool zc_espnow_pairing(void);
uint8_t zc_espnow_peer_count(void);
/** Drop every stored peer — back to the open broadcast kit. */
void zc_espnow_forget_peers(void);
