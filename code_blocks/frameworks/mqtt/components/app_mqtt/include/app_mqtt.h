/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - MQTT (Home Assistant) Framework
 *
 * BASE: No-op stub. The generator replaces app_mqtt.cpp with the real
 * implementation when the product has device-type bindings.
 */

#pragma once

#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_mqtt_init(void);

/* ── Configuration ──────────────────────────────────────────────────────
 *
 * What the mqtt-* console commands do, as functions — for a product that
 * onboards some other way. Each validates, persists to NVS and applies at once
 * (a broker change restarts the client). The NVS layout stays this block's
 * business. The block already ships a guided `mqtt-setup` command; a product
 * wanting a different flow writes a console command over these setters and
 * app_console_read_line (app_console.h), not a second copy of the storage.
 */

/** Wi-Fi network. ESP_ERR_NOT_SUPPORTED when Matter/RainMaker owns Wi-Fi in
 *  this product (it is onboarded through them); ESP_ERR_INVALID_SIZE when the
 *  SSID is over 32 bytes or the password over 64. */
esp_err_t app_mqtt_set_wifi(const char *ssid, const char *password);

/** Broker, e.g. "mqtt://192.168.1.10". Username / password may be NULL or ""
 *  for none. URI over 128 bytes, username over 32 or password over 64:
 *  ESP_ERR_INVALID_SIZE. Restarts the MQTT client, so never call it from an
 *  MQTT event handler (it runs on the client's own task, which the restart
 *  stops) — call it from a console command or an app task. */
esp_err_t app_mqtt_set_broker(const char *uri, const char *username, const char *password);

/** Everything needed to connect is stored (Wi-Fi when this block owns it, and
 *  the broker). */
bool app_mqtt_configured(void);

/** Whether this block owns Wi-Fi (false when Matter/RainMaker is co-selected). */
bool app_mqtt_owns_wifi(void);

#ifdef __cplusplus
}
#endif
