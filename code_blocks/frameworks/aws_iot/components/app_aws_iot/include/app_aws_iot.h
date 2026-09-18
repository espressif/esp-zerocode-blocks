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
