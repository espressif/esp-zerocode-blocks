/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_NOT_SUPPORTED 0x106
static inline const char *esp_err_to_name(esp_err_t e){ (void)e; return "ESP_ERR"; }
