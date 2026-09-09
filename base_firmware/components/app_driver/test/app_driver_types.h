/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Host-test stand-in for the GENERATED app_driver_types.h. Mirrors the union
 * + handle/source shape the generator emits; params are a minimal fixed set. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef enum {
    APP_DRIVER_PARAM_NONE = 0,
    APP_DRIVER_PARAM_POWER,       /* .b */
    APP_DRIVER_PARAM_LED_PATTERN, /* .u8 */
    APP_DRIVER_PARAM_LEVEL,       /* .u16 */
    APP_DRIVER_PARAM_MAX
} app_driver_param_id_t;
typedef union { bool b; uint8_t u8; int16_t i16; uint16_t u16; uint32_t u32; } app_driver_param_val_t;
#define APP_DRIVER_MAX_SOLUTIONS 8
typedef uint8_t app_driver_handle_t;
#define APP_DRIVER_SOURCE_LOCAL 0
typedef void (*app_driver_notify_cb_t)(app_driver_param_id_t, app_driver_param_val_t, app_driver_handle_t, void *);
