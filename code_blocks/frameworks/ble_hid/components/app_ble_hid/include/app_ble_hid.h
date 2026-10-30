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

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_ble_hid_init(void);

/**
 * Send a consumer-control usage (press + release) to the paired host.
 * Usage codes are USB HID Consumer Page (0x0C): 0xCD play/pause,
 * 0xE9 volume up, 0xEA volume down, 0xE2 mute, 0xB5 next, 0xB6 previous.
 * No-op stub when the product carries no bindings; safe before pairing
 * (drops the report if no host is connected).
 */
void zc_ble_hid_send_consumer(uint16_t usage);

/**
 * Send a keyboard key (press + release) to the paired host: a USB HID
 * Keyboard Page keycode — 0x4F right arrow, 0x50 left arrow, 0x4B page up,
 * 0x4E page down, 0x2C space — with modifier bits (0x02 left shift, 0x01
 * left control). Dropped when no host is connected.
 */
void zc_ble_hid_send_key(uint8_t keycode, uint8_t modifiers);

/**
 * The gamepad's whole state (report id 3): button n is bit n-1 of `buttons`
 * (16 buttons), and the two sticks' axes x, y (left) and z, rz (right) are
 * -127..127, 0 centred. A state, not a press: call it when anything changed,
 * at the rate the product reads its inputs. Dropped while no host is
 * connected. Defined only when the product composes device_types/gamepad.
 */
void zc_ble_hid_send_gamepad(uint16_t buttons, int8_t x, int8_t y, int8_t z, int8_t rz);

#ifdef __cplusplus
}
#endif
