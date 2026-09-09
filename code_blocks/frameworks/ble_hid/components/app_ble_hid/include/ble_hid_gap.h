/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** BLE-only NimBLE GAP helper for the BLE HID framework — see ble_hid_gap.c */
#pragma once

#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ble_hid_gap_stack_init(void);
esp_err_t ble_hid_gap_adv_init(uint16_t appearance, const char *device_name);
esp_err_t ble_hid_gap_adv_start(void);
esp_err_t ble_hid_gap_start_host(void);

#ifdef __cplusplus
}
#endif
