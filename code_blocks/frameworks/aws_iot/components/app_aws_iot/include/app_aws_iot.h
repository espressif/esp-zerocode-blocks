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
 * NOT STABLE AT INIT. Until the device is onboarded this is a MAC-derived
 * default (zerocode-<mac>), and onboarding happens over the console long after
 * every logic_init slot has run. Build topics from it on ZC_AWS_EVENT_CONNECTED
 * rather than caching them at init, or a device will subscribe to the default
 * name, report a healthy session, and receive nothing.
 */
const char *app_aws_iot_thing_name(void);

/** Whether the MQTT session is currently up. */
bool app_aws_iot_connected(void);

/* ── Configuration ──────────────────────────────────────────────────────
 *
 * What the aws-* console commands do, as functions: for a product that
 * onboards some other way (its own console command, a web form, values set at
 * build time). Each setter validates, persists to NVS and updates the live
 * config — the session task picks it up on its next connect attempt (it polls
 * once a second while unconfigured). The NVS namespace and key names stay this
 * block's business: never write them directly.
 *
 * The block already ships a guided setup, `aws-setup`, that asks for every
 * value in turn. A product that wants a different flow writes a console
 * command over these setters and app_console_read_line / app_console_read_text
 * (app_console.h) — not a second copy of the storage.
 */

/** Wi-Fi network. ESP_ERR_INVALID_SIZE when the SSID is over 32 bytes or the
 *  password over 64. Stored even when another framework owns the radio (see
 *  app_aws_iot_owns_wifi), but only applied when this block owns it. */
esp_err_t app_aws_iot_set_wifi(const char *ssid, const char *password);

/** AWS IoT data endpoint, e.g. a1b2c3-ats.iot.eu-west-1.amazonaws.com.
 *  @param port 8883 (MQTT over TLS) or 443 (ALPN); 0 means 8883. */
esp_err_t app_aws_iot_set_endpoint(const char *host, uint16_t port);

/** Thing name — also the MQTT client id. Over 64 bytes: ESP_ERR_INVALID_SIZE. */
esp_err_t app_aws_iot_set_thing(const char *name);

/** Device certificate / private key / root CA override, as NUL-TERMINATED PEM
 *  text (no length argument, on purpose: mbedTLS wants the length including the
 *  NUL, and strlen() is the classic way to get that wrong). Each is parsed with
 *  mbedTLS first and refused with ESP_ERR_INVALID_ARG if it does not parse.
 *  app_aws_iot_set_root_ca(NULL) drops the override (back to Amazon Root CA 1). */
esp_err_t app_aws_iot_set_cert(const char *pem);
esp_err_t app_aws_iot_set_key(const char *pem);
esp_err_t app_aws_iot_set_root_ca(const char *pem);

/**
 * Last will: what the broker publishes for this device when it drops off
 * WITHOUT a clean disconnect — a power cut, a crash, a network loss — so a
 * subscriber learns the device is gone instead of showing its last state for
 * ever. Typical: app_aws_iot_set_will("state/{thing}", "offline", 1, true),
 * matching a retained state topic the product publishes "online" to.
 *
 * @param topic   "{thing}" anywhere in it is replaced with the thing name AT
 *                EACH CONNECT. Do not build the topic from
 *                app_aws_iot_thing_name() yourself: until the device is
 *                onboarded that is the MAC default, and a topic built at init
 *                would announce "offline" where nobody listens. NULL clears the
 *                will. At most 127 bytes before substitution.
 * @param payload NUL-terminated, at most 255 bytes. Copied, like the topic.
 * @param qos     0 or 1 (AWS IoT Core has no QoS 2).
 * @param retain  Usually true, to match a retained state topic.
 *
 * The will is part of the MQTT CONNECT, so it reaches the broker at the next
 * connect — set it at init (any time before the first connect), not mid-session.
 * Not persisted: call it on every boot.
 */
esp_err_t app_aws_iot_set_will(const char *topic, const char *payload, uint8_t qos, bool retain);

/** Erase the stored certificate, private key and root CA override. */
esp_err_t app_aws_iot_clear_credentials(void);

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
