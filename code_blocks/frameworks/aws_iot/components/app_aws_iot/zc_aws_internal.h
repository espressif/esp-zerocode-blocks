/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Shared between this component's translation units. Not installed — nothing
 * outside app_aws_iot includes it.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <esp_err.h>
#include <freertos/FreeRTOS.h>

#include <core_mqtt.h>
#include <core_mqtt_agent.h>

#include "app_aws_iot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* coreMQTT's packet buffer is the MAXIMUM MQTT packet, topic included — a
 * larger packet is a hard failure, not a truncation. 2 KB carries a generous
 * application payload plus the topic; raise it here if a product needs more. */
#define ZC_AWS_NET_BUF_SIZE     2048
#define ZC_AWS_KEEPALIVE_S      60
#define ZC_AWS_CONNACK_MS       5000
#define ZC_AWS_BACKOFF_BASE_MS  1000
#define ZC_AWS_BACKOFF_MAX_MS   32000
/* A session shorter than this did not "work": the broker accepted the CONNECT
 * and then dropped the client (a policy refusing a publish or the will, a
 * duplicate client id). Treated as a failed connect — backed off, not retried
 * at once — or such a device reconnects as fast as TLS allows, forever. */
#define ZC_AWS_STABLE_SESSION_MS 10000
#define ZC_AWS_MAX_SUBS         8
#define ZC_AWS_CMD_QUEUE_LEN    16

extern const char *ZC_AWS_TAG;

/* ── configuration, owned by app_aws_iot.cpp ─────────────────────────── */
extern char     g_zc_aws_ssid[33];
extern char     g_zc_aws_pass[65];
extern char     g_zc_aws_endpoint[129];
extern char     g_zc_aws_thing[65];
extern int32_t  g_zc_aws_port;
extern char    *g_zc_aws_cert;   extern size_t g_zc_aws_cert_len;
extern char    *g_zc_aws_key;    extern size_t g_zc_aws_key_len;
extern char    *g_zc_aws_rootca; extern size_t g_zc_aws_rootca_len;
extern const char ZC_AWS_ROOT_CA[];
extern size_t   zc_aws_root_ca_size(void);

extern volatile bool g_zc_aws_net_up;
extern volatile bool g_zc_aws_connected;
extern volatile bool g_zc_aws_wifi_owned;   /* true when THIS block started Wi-Fi */
extern volatile bool g_zc_aws_wifi_decided; /* the ownership decision above has been made */

bool zc_aws_configured(void);
void zc_aws_load_config(void);
/* The live config lock. Held by the session task across a connect — the TLS
 * The live config lock. Taken BRIEFLY: by every setter while it changes the live
 * config, and by the session task while it copies that config for one connect
 * (the connect then runs on its own copy, so a setter never waits for a TLS
 * handshake, and never frees a buffer the handshake is reading). Not
 * recursive: zc_aws_load_config() must be called with it held, never take it. */
void zc_aws_cfg_lock(void);
void zc_aws_cfg_unlock(void);
/* A setter changed the config: wake the session task out of a reconnect
 * backoff so the fix is tried at once (zc_aws_agent.cpp). */
void zc_aws_config_changed(void);
void zc_aws_wifi_reconnect(void);           /* apply g_zc_aws_ssid/pass now */
/* Claim or defer Wi-Fi ownership. Called ONCE from the session task, not from
 * init() — see the comment on wifi_claim_or_defer() for why the timing matters. */
void zc_aws_net_start(void);
void zc_aws_post_event(int32_t event_id);

/* ── subscription registry, zc_aws_subs.cpp ──────────────────────────── */
void      zc_aws_subs_init(void);
esp_err_t zc_aws_subs_add(const char *filter, uint8_t qos,
                          zc_aws_msg_cb_t cb, void *ctx);
void      zc_aws_subs_dispatch(MQTTPublishInfo_t *pub);
size_t    zc_aws_subs_count(void);
/* Snapshot entry i. Returns false once i is past the end. */
bool      zc_aws_subs_get(size_t i, const char **filter, uint16_t *len, uint8_t *qos);

/* ── session, zc_aws_agent.cpp ───────────────────────────────────────── */
esp_err_t zc_aws_agent_start(void);
/* Enqueue SUBSCRIBE for one registry entry; used at connect and by a late
 * app_aws_iot_subscribe(). No-op when the session is down. */
void      zc_aws_agent_send_subscribe(size_t index);

/* ── console, zc_aws_console.cpp ─────────────────────────────────────── */
void zc_aws_console_register(void);
/* Does NUL-terminated PEM text parse as a certificate ("cert"/"rootca") or a
 * private key ("key")? Logs the mbedTLS code when not. zc_aws_config.cpp. */
bool zc_aws_pem_parses(const char *tag, const char *pem);
/* The will for THIS connect (app_aws_iot_set_will, "{thing}" substituted with
 * `thing`), or false for none. Topic and payload are COPIED into the caller's
 * buffers under the will's lock, so a concurrent set_will cannot tear them.
 * zc_aws_config.cpp; called by the session task right before MQTT_Connect. */
bool zc_aws_will_for_connect(MQTTPublishInfo_t *out, const char *thing,
                             char *topic_buf, size_t topic_cap, char *payload_buf, size_t payload_cap);

#ifdef __cplusplus
}
#endif
