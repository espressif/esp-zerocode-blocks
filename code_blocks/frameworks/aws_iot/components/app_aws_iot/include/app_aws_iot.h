/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - AWS IoT Core connectivity
 *
 * One mutual-TLS MQTT session to the product owner's own AWS account, usable
 * from any task. This component is a PIPE: it knows nothing about device types,
 * params, shadows or payload encodings.
 *
 * Thread safety: every function here may be called from any task. The session
 * is owned by a single task and reached through coreMQTT-Agent's command queue.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <esp_err.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>

#include <core_mqtt_agent.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Connection-state events. Any behaviour can hook these from a logic_init slot
 *  with esp_event_handler_register() — no framework-specific slot required. */
ESP_EVENT_DECLARE_BASE(ZC_AWS_EVENT);

enum {
    ZC_AWS_EVENT_CONNECTED = 0,   /**< session up; subscriptions are in place */
    ZC_AWS_EVENT_DISCONNECTED,    /**< session lost; the block is reconnecting */
};

/**
 * Called on the agent task for each message matching a subscribed filter.
 *
 * @param topic    NOT NUL-terminated — use @p topic_len.
 * @param payload  Valid only for the duration of the call; copy what you keep.
 */
typedef void (*zc_aws_msg_cb_t)(const char *topic, uint16_t topic_len,
                                const void *payload, size_t len, void *ctx);

/**
 * Bring up the network and start the session task.
 *
 * Never blocks: a device with no credentials logs what is missing and idles, so
 * boot always completes and CI can build a product with no AWS account.
 */
esp_err_t app_aws_iot_init(void);

/**
 * Publish, from any task.
 *
 * The topic and payload are COPIED before the call returns, so the caller's
 * buffers need not outlive it — coreMQTT-Agent itself does not copy, and a
 * queued command referencing a stack buffer is a use-after-free that only shows
 * up under load.
 *
 * @param qos    0 or 1. AWS IoT Core does not support QoS 2.
 * @param block  How long to wait for room in the command queue.
 * @return ESP_ERR_INVALID_STATE if the session is not up, ESP_ERR_NO_MEM if the
 *         copy fails, ESP_ERR_TIMEOUT if the queue stays full.
 */
esp_err_t app_aws_iot_publish(const char *topic, const void *payload, size_t len,
                              uint8_t qos, TickType_t block);

/**
 * app_aws_iot_publish with the MQTT RETAIN flag: the broker keeps the message
 * and hands it to every later subscriber. For STATE ("online", the current
 * mode), never for events. A retained last will (app_aws_iot_set_will) needs
 * its "online" counterpart published this way, or a reconnect leaves the
 * retained "offline" standing.
 */
esp_err_t app_aws_iot_publish_retained(const char *topic, const void *payload, size_t len,
                                       uint8_t qos, TickType_t block);

/**
 * Subscribe, from any task.
 *
 * DECLARATIVE: the filter is recorded (and copied) and re-sent on every
 * reconnect where the broker reports no existing session. Registering once at
 * init is therefore correct and stays correct across network drops.
 *
 * Safe to call before the session is up; it takes effect at connect.
 *
 * @return ESP_ERR_NO_MEM if the registry is full (ZC_AWS_MAX_SUBS).
 */
esp_err_t app_aws_iot_subscribe(const char *topic_filter, uint8_t qos,
                                zc_aws_msg_cb_t cb, void *ctx);

/**
 * The configured thing name — also the MQTT client id, and what shadow and jobs
 * topics are built from. Never NULL.
 *
 * The flashed device data's, read by app_aws_iot_init() and so final by the
 * time any logic_init slot runs; a MAC-derived default (zerocode-<mac>) on a
 * device flashed without one.
 */
const char *app_aws_iot_thing_name(void);

/** Whether the MQTT session is currently up. */
bool app_aws_iot_connected(void);

/* ── Configuration ──────────────────────────────────────────────────────
 *
 * The AWS identity — endpoint, port, thing name, certificate, private key and
 * an optional root CA — is device data: the platform's flash page asks for it
 * and writes it to the fctry partition (block.yml device_data:). It is read
 * once, at init, and has no setter.
 *
 * Wi-Fi is the one value set on the device. The block ships `aws-setup`
 * (guided) and `aws-wifi`; a product that wants a different flow writes a
 * console command over app_aws_iot_set_wifi and app_console_read_line
 * (app_console.h) — not a second copy of the storage.
 */

/** Wi-Fi network: validates, persists to NVS and applies it. The session task
 *  picks it up at once. ESP_ERR_INVALID_SIZE when the SSID is over 32 bytes or
 *  the password over 64. Stored even when another framework owns the radio
 *  (see app_aws_iot_owns_wifi), but only applied when this block owns it. */
esp_err_t app_aws_iot_set_wifi(const char *ssid, const char *password);

/**
 * Last will: what the broker publishes for this device when it drops off
 * WITHOUT a clean disconnect — a power cut, a crash, a network loss — so a
 * subscriber learns the device is gone instead of showing its last state for
 * ever. Typical: app_aws_iot_set_will("state/{thing}", "offline", 1, true),
 * matching a retained state topic the product publishes "online" to.
 *
 * @param topic   "{thing}" anywhere in it is replaced with the thing name at
 *                each connect. NULL clears the will. At most 127 bytes before
 *                substitution.
 * @param payload NUL-terminated, at most 255 bytes. Copied, like the topic.
 * @param qos     0 or 1 (AWS IoT Core has no QoS 2).
 * @param retain  Usually true, to match a retained state topic.
 *
 * The will is part of the MQTT CONNECT, so it reaches the broker at the next
 * connect — set it at init (any time before the first connect), not mid-session.
 * Not persisted: call it on every boot.
 */
esp_err_t app_aws_iot_set_will(const char *topic, const char *payload, uint8_t qos, bool retain);

/** Endpoint, thing name, certificate and key are all present. */
bool app_aws_iot_configured(void);

/** Whether THIS block brought up Wi-Fi. False when another framework
 *  (RainMaker, Matter) owns the radio — Wi-Fi is then onboarded through it —
 *  and false for the first ~2 s after boot, before the decision is made. */
bool app_aws_iot_owns_wifi(void);

/**
 * The live agent context, for AWS libraries that must join this session —
 * Device Shadow, Jobs, OTA over MQTT file streams, Device Defender.
 *
 * NULL until the first successful connect. Use the wrapped calls above unless
 * a library specifically demands the handle: they are the misuse-resistant path.
 */
MQTTAgentContext_t *app_aws_iot_agent(void);

#ifdef __cplusplus
}
#endif
