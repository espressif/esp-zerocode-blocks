/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - BLE HID (media remote) Framework
 *
 * BASE: No-op stub. The generator replaces app_ble_hid.cpp with the real
 * implementation when the product has device-type bindings.
 */

#pragma once

#include <esp_err.h>
#include <stdint.h>

esp_err_t app_ble_hid_init(void);

/**
 * Send a consumer-control usage (press + release) to the paired host.
 * Usage codes are USB HID Consumer Page (0x0C): 0xCD play/pause,
 * 0xE9 volume up, 0xEA volume down, 0xE2 mute, 0xB5 next, 0xB6 previous.
 * No-op stub when the product carries no bindings; safe before pairing
 * (drops the report if no host is connected).
 */
void zc_ble_hid_send_consumer(uint16_t usage);
