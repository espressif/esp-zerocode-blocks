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
#include <stdint.h>
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

/* ── Product entities ───────────────────────────────────────────────────
 *
 * Home Assistant entities the product adds itself, beside the catalog device
 * types: they appear on the same Home Assistant device and share its
 * availability. Every call below is safe from any task and never waits on
 * the network: publishes are queued and sent in order by the framework.
 *
 * Topics (node = station MAC, lowercase hex):
 *     zerocode/<node>/product/<object_id>/state   retained state
 *     zerocode/<node>/product/<object_id>/set     commands (commandable kinds)
 */

/** Command callback: runs on the MQTT client's task, so it must not block.
 *  `payload` is NUL-terminated and valid only during the call. */
typedef void (*app_mqtt_command_cb_t)(const char *object_id, const char *payload, void *ctx);

/** Announce an entity through Home Assistant discovery, now if connected and
 *  again on every (re)connect. `component` is the Home Assistant kind
 *  ("sensor", "binary_sensor", "switch", "number", "button", "select", …),
 *  `object_id` is [a-z0-9_], at most 32 characters and unique in the product,
 *  `name` is the display name (at most 64). `extra_json` (NULL for none) adds
 *  discovery members, with or without surrounding braces, e.g.
 *  "\"device_class\":\"moisture\"" or
 *  "{\"unit_of_measurement\":\"%\",\"min\":0,\"max\":100}".
 *  state_topic is set for every kind except button; command_topic for switch,
 *  number, button, select, text, light, fan, cover, lock, siren and valve.
 *  Calling it again with the same object_id replaces the announcement.
 *  ESP_ERR_INVALID_ARG on a bad id or name, ESP_ERR_NO_MEM when the table
 *  (APP_MQTT_MAX_ENTITIES, default 16) is full. */
esp_err_t app_mqtt_entity_add(const char *component, const char *object_id, const char *name,
                              const char *extra_json);

/** Set an entity's state, e.g. "ON" / "OFF" for a binary_sensor or switch,
 *  "42.5" for a sensor or number. Published retained (QoS 1) when connected
 *  and the value changed; always kept, and republished on every (re)connect
 *  and, with app_mqtt_set_republish_s, on that schedule — so an unchanged
 *  value never needs sending again from product code.
 *  May be called before app_mqtt_entity_add for that id (a sensor's first
 *  reading usually arrives before the product adds its entities): the latest
 *  value is kept and becomes the entity's first state when it is added, sent
 *  as any first value is. Up to 8 ids wait like that. An id still not added
 *  when the broker connects gets an error logged, once per id.
 *  ESP_ERR_INVALID_ARG on a bad id; ESP_ERR_NOT_FOUND when an id not yet
 *  added cannot wait (8 already do, or no room), with an error logged once. */
esp_err_t app_mqtt_entity_set_state(const char *object_id, const char *state);

/** Receive commands for a commandable entity (switch: "ON"/"OFF", number: the
 *  value, button: "PRESS", select: the option). One callback per entity, may
 *  be registered before or after app_mqtt_entity_add; NULL removes it. The
 *  callback reports the result with app_mqtt_entity_set_state. A callback
 *  whose id has no entity by the time the broker connects never receives a
 *  command; that is logged as an error, once per id. */
esp_err_t app_mqtt_entity_on_command(const char *object_id, app_mqtt_command_cb_t cb, void *ctx);

/** Re-send every entity's current retained state — device types and product
 *  entities alike — every `seconds` (0 = off, the default). For a reading that
 *  must reach the broker on a schedule even while it does not change ("report
 *  every 10 minutes"). The values are recorded from this call on, so call it
 *  before the broker connects: behaviors/mqtt_republish does, at init. Re-sent
 *  by a task of this framework's own; ESP_ERR_NO_MEM when it cannot start. */
esp_err_t app_mqtt_set_republish_s(uint32_t seconds);

/** With a republish interval set, true stops state topics being sent on
 *  change: each one's latest value is sent only on the schedule, plus its
 *  first value since boot and on every (re)connect, so Home Assistant always
 *  has one. A state changed by a command arrives at once. Discovery,
 *  availability and non-retained (event) messages are never held. For "report
 *  every N minutes". Off by default; call it before app_mqtt_set_republish_s. */
void app_mqtt_set_republish_only_on_schedule(bool only);

/* ── Product topics ─────────────────────────────────────────────────────
 *
 * The product's own topics, on the framework's one connection: a second
 * MQTT client to the same broker gets the same default client id and the two
 * knock each other off forever. Safe from any task; never waits on the
 * network. */

/** Message callback: runs on the MQTT client's task, so it must not block.
 *  `topic` and `payload` are NUL-terminated copies valid only during the call
 *  (`len` is the payload's length; it may contain NULs). */
typedef void (*app_mqtt_topic_cb_t)(const char *topic, const char *payload, int len, void *ctx);

/** Publish to any topic (queued, sent in order). `len` 0 means strlen.
 *  ESP_ERR_INVALID_STATE while not connected — nothing is kept for later:
 *  a value that must reach the broker after a reconnect is an entity state
 *  (app_mqtt_entity_set_state) or is published again from app_mqtt_on_connected. */
esp_err_t app_mqtt_publish(const char *topic, const char *payload, int len, int qos, bool retain);

/** Receive messages on `filter` (MQTT wildcards: '+' one level, '#' the
 *  rest). Subscribed now if connected and again on every (re)connect.
 *  Registering a filter again replaces its callback. ESP_ERR_NO_MEM when
 *  APP_MQTT_MAX_TOPIC_SUBS (default 8) filters are registered. */
esp_err_t app_mqtt_subscribe(const char *filter, int qos, app_mqtt_topic_cb_t cb, void *ctx);

/** Called on every (re)connect after the framework has announced its device
 *  types and the product entities and published their state. The framework's
 *  definition is weak and empty: a product defines its own in app_logic
 *  (linked whole, so the override is always picked up). Runs on the MQTT
 *  client's task, so it must not block. */
void app_mqtt_on_connected(void);

#ifdef __cplusplus
}
#endif
