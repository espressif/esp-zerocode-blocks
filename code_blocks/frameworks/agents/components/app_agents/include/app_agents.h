/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <esp_err.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Bring up the Agents client: console commands (agent-wifi / agent-id /
 *  agent-token / agent-say / agent-new), NVS-persisted config, and the
 *  websocket conversation once network + credentials are present. Never
 *  blocks app_main. */
esp_err_t app_agents_init(void);
#ifdef __cplusplus
}
#endif
