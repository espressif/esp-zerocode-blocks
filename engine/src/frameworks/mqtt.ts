// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { Product, RenderedInstance } from '../types.js'
import { type CatalogFeatures, wifiShared, collectSlot, CURRENT_CATALOG, refusalShim, indentText, unwrapInitBody } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

/**
 * app_mqtt.cpp — MQTT with Home Assistant discovery.
 *
 * Sibling of the other genApp*Cpp functions. The binding: each device-type
 * instance's mqtt_device_create publishes a retained HA discovery config,
 * subscribes its command topic, and publishes initial state; incoming
 * MQTT_EVENT_DATA dispatches through mqtt_command_cases (topic/payload in
 * scope); param-bus changes flow out through mqtt_driver_cb_cases.
 *
 * Wi-Fi lives HERE (like RainMaker's app_network) because a bare MQTT node
 * has no phone-app provisioning: creds + broker URI come from the serial
 * console (mqtt-wifi / mqtt-broker) and persist in NVS. An unconfigured
 * boot idles and logs — it must never block app_main.
 */
export function genAppMqttCpp(rendered: RenderedInstance[], features: CatalogFeatures = CURRENT_CATALOG): string {
  // SHARED-NETWORK MODE: when a Wi-Fi-owning transport (Matter / RainMaker)
  // is co-selected, app_mqtt must NOT own Wi-Fi — no esp_wifi init, no
  // credentials, no mqtt-wifi console command. It waits for the transport's
  // network instead (IP_EVENT_STA_GOT_IP on the default event loop, plus an
  // already-has-IP check for the reboot-while-provisioned case) and starts
  // the MQTT client when connectivity appears. This is what makes
  // frameworks: [matter, mqtt] (a Matter device that also speaks Home
  // Assistant) composable.
  const sharedNet = wifiShared(rendered, 'mqtt')
  const usedDev = new Set<string>()
  const devFns: string[] = []
  const devCalls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['mqtt_device_create'] ?? '').trim()) continue
    const slug = (r.prefix || r.block.id.split('/').pop() || 'dev').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; usedDev.has(name); n++) name = `${slug}_${n}`
    usedDev.add(name)
    devFns.push(`static void zc_${name}_mqtt_device_init(void)\n{\n${indentText(unwrapInitBody(r.slots['mqtt_device_create']), 4)}\n}`)
    devCalls.push(`    zc_${name}_mqtt_device_init();`)
  }
  return `#include "app_mqtt.h"
#include "app_driver.h"
#include "app_config.h"
#include "app_console.h"

#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_console.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <nvs.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mqtt_client.h>
${collectSlot(rendered, 'mqtt_includes')}

/* The product API this file defines, declared as app_mqtt.h declares it: an
 * older app_mqtt.h has none of it, and C linkage must not depend on which. */
extern "C" {
typedef void (*app_mqtt_command_cb_t)(const char *object_id, const char *payload, void *ctx);
typedef void (*app_mqtt_topic_cb_t)(const char *topic, const char *payload, int len, void *ctx);
esp_err_t app_mqtt_entity_add(const char *component, const char *object_id, const char *name,
                              const char *extra_json);
esp_err_t app_mqtt_entity_set_state(const char *object_id, const char *state);
esp_err_t app_mqtt_entity_on_command(const char *object_id, app_mqtt_command_cb_t cb, void *ctx);
esp_err_t app_mqtt_set_republish_s(uint32_t seconds);
void app_mqtt_set_republish_only_on_schedule(bool only);
esp_err_t app_mqtt_publish(const char *topic, const char *payload, int len, int qos, bool retain);
esp_err_t app_mqtt_subscribe(const char *filter, int qos, app_mqtt_topic_cb_t cb, void *ctx);
void app_mqtt_on_connected(void);
}

static const char *TAG = "app_mqtt";
${refusalShim(features)}
static app_driver_handle_t s_handle = 0;
static esp_mqtt_client_handle_t s_client = NULL;
static volatile bool s_connected = false;
static bool s_wifi_started = false;

/* Node identity: station MAC as lowercase hex — stable, unique per board. */
static char s_node_id[13] = {0};
static char s_status_topic[48] = {0};

/* NVS-backed config (namespace "zc_mqtt"), set via the console commands. */
${sharedNet ? '' : `static char s_ssid[33] = {0};
static char s_pass[65] = {0};
`}static char s_uri[129] = {0};
static char s_user[33] = {0};
static char s_mqtt_pass[65] = {0};

/* ── Outgoing messages ──────────────────────────────────────────────────
 * Every publish goes through zc_mqtt_publish, which only queues: the callers
 * include param-bus callbacks on the esp_timer task, which must never wait on
 * the network. One task sends the queue in order. */
#define ZC_MQTT_TX_DEPTH 64

typedef struct {
    bool subscribe;
    bool retain;
    int qos;
    int len;
    char *topic;
    char *data;
} zc_mqtt_tx_t;

static QueueHandle_t s_tx_q = NULL;
static SemaphoreHandle_t s_client_lock = NULL;  /* held while s_client is used or replaced */
static TaskHandle_t s_mqtt_task = NULL;         /* the client's own task */
static volatile int s_tx_pending = 0;

static bool zc_on_mqtt_task(void)
{
    return s_mqtt_task != NULL && xTaskGetCurrentTaskHandle() == s_mqtt_task;
}

static int zc_mqtt_send(const zc_mqtt_tx_t *m)
{
    if (m->subscribe) return esp_mqtt_client_subscribe(s_client, m->topic, m->qos);
    return esp_mqtt_client_publish(s_client, m->topic, m->data, m->len, m->qos, m->retain);
}

static void zc_mqtt_tx_task(void *arg)
{
    (void)arg;
    for (;;) {
        zc_mqtt_tx_t *m = NULL;
        if (xQueueReceive(s_tx_q, &m, portMAX_DELAY) != pdTRUE) continue;
        xSemaphoreTake(s_client_lock, portMAX_DELAY);
        if (s_client != NULL) zc_mqtt_send(m);
        xSemaphoreGive(s_client_lock);
        free(m);
        __atomic_sub_fetch(&s_tx_pending, 1, __ATOMIC_SEQ_CST);
    }
}

static int zc_mqtt_queue(bool subscribe, const char *topic, const char *data, int len, int qos, bool retain)
{
    if (s_client == NULL || topic == NULL) return -1;
    size_t dl = 0;
    if (!subscribe && data != NULL) dl = len > 0 ? (size_t)len : strlen(data);
    size_t tl = strlen(topic) + 1;
    zc_mqtt_tx_t *m = (zc_mqtt_tx_t *)malloc(sizeof(*m) + tl + dl + 1);
    if (m == NULL) {
        ESP_LOGW(TAG, "out of memory, message for %s dropped", topic);
        return -1;
    }
    m->subscribe = subscribe;
    m->retain = retain;
    m->qos = qos;
    m->len = (int)dl;
    m->topic = (char *)(m + 1);
    memcpy(m->topic, topic, tl);
    m->data = m->topic + tl;
    if (dl) memcpy(m->data, data, dl);
    m->data[dl] = 0;
    __atomic_add_fetch(&s_tx_pending, 1, __ATOMIC_SEQ_CST);
    if (s_tx_q != NULL && xQueueSend(s_tx_q, &m, 0) == pdTRUE) return 0;
    __atomic_sub_fetch(&s_tx_pending, 1, __ATOMIC_SEQ_CST);
    int ret = -1;
    if (zc_on_mqtt_task()) {
        /* The client's own task cannot wait for the queue it feeds. */
        ret = zc_mqtt_send(m);
    } else {
        ESP_LOGW(TAG, "send queue full, message for %s dropped", topic);
    }
    free(m);
    return ret < 0 ? -1 : 0;
}

/* ── Republish (app_mqtt_set_republish_s) ───────────────────────────────
 * A state is published when it CHANGES, so a steady reading is sent once and
 * then never again. With a republish interval set, the last retained message
 * on every state topic is kept here and re-sent on that schedule by a task of
 * this framework's own. With "only on schedule" a state topic is not sent on
 * change at all: its latest value waits for the schedule, except its first
 * value since boot and anything sent from the MQTT client's own task (every
 * (re)connect's announcement, a command's result), which go at once. */
/* zc-retained: begin */
#ifndef APP_MQTT_MAX_RETAINED
#define APP_MQTT_MAX_RETAINED 32
#endif
#define ZC_RETAINED_MAX_LEN 1024

typedef struct {
    char *topic; /* one allocation: topic, NUL, data, NUL */
    char *data;
    int len;
    int qos;
    bool sent; /* some value of this topic has been queued since boot */
} zc_retained_t;

typedef struct {
    zc_retained_t e[APP_MQTT_MAX_RETAINED];
    int n;
} zc_retained_set_t;

typedef enum {
    ZC_RETAINED_STORED,
    ZC_RETAINED_FULL,
    ZC_RETAINED_TOO_LONG,
    ZC_RETAINED_NO_MEM,
} zc_retained_put_t;

/* Keep data as the latest retained message on topic. An empty message clears
 * a topic's retained value on the broker, so it leaves nothing to re-send.
 * The caller holds the lock. */
static zc_retained_put_t zc_retained_put(zc_retained_set_t *s, const char *topic, const char *data, int len, int qos)
{
    if (len < 0 || len > ZC_RETAINED_MAX_LEN) return ZC_RETAINED_TOO_LONG;
    int at = -1;
    for (int i = 0; i < s->n; i++) {
        if (strcmp(s->e[i].topic, topic) == 0) {
            at = i;
            break;
        }
    }
    if (len == 0) {
        if (at >= 0) {
            free(s->e[at].topic);
            s->e[at] = s->e[--s->n];
        }
        return ZC_RETAINED_STORED;
    }
    if (at < 0 && s->n >= APP_MQTT_MAX_RETAINED) return ZC_RETAINED_FULL;
    size_t tl = strlen(topic) + 1;
    char *buf = (char *)malloc(tl + (size_t)len + 1);
    if (buf == NULL) return ZC_RETAINED_NO_MEM;
    memcpy(buf, topic, tl);
    memcpy(buf + tl, data, (size_t)len);
    buf[tl + (size_t)len] = 0;
    if (at < 0) {
        at = s->n++;
        s->e[at].sent = false;
    } else {
        free(s->e[at].topic);
    }
    s->e[at].topic = buf;
    s->e[at].data = buf + tl;
    s->e[at].len = len;
    s->e[at].qos = qos;
    return ZC_RETAINED_STORED;
}

static zc_retained_t *zc_retained_find(zc_retained_set_t *s, const char *topic)
{
    for (int i = 0; i < s->n; i++) {
        if (strcmp(s->e[i].topic, topic) == 0) return &s->e[i];
    }
    return NULL;
}

static void zc_retained_clear(zc_retained_set_t *s)
{
    for (int i = 0; i < s->n; i++) free(s->e[i].topic);
    s->n = 0;
}
/* zc-retained: end */

static zc_retained_set_t s_retained;
static volatile uint32_t s_republish_s = 0;
static volatile bool s_republish_only = false;
static TaskHandle_t s_republish_task = NULL;
static bool s_retained_warned = false;

static SemaphoreHandle_t zc_retained_lock(void)
{
    static StaticSemaphore_t buf;
    static SemaphoreHandle_t h = NULL;
    static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    if (h == NULL) {
        taskENTER_CRITICAL(&mux);
        if (h == NULL) h = xSemaphoreCreateMutexStatic(&buf);
        taskEXIT_CRITICAL(&mux);
    }
    return h;
}

/* Re-sent state: every retained message but Home Assistant discovery and the
 * availability topic. */
static bool zc_retained_tracked(const char *topic)
{
    return strncmp(topic, "homeassistant/", 14) != 0 && strcmp(topic, s_status_topic) != 0;
}

/* Records a state message and says whether to send it now: always, unless
 * held for the schedule — held when "only on schedule" is on, the message is
 * not from the MQTT client's own task, and this topic already had a value
 * sent. A message that cannot be recorded is sent, never lost. */
static bool zc_retained_record(const char *topic, const char *data, int len, int qos)
{
    int dl = data == NULL ? 0 : (len > 0 ? len : (int)strlen(data));
    bool hold = s_republish_only && !zc_on_mqtt_task();
    bool send = true;
    xSemaphoreTake(zc_retained_lock(), portMAX_DELAY);
    zc_retained_put_t r = zc_retained_put(&s_retained, topic, data, dl, qos);
    zc_retained_t *e = r == ZC_RETAINED_STORED && dl > 0 ? zc_retained_find(&s_retained, topic) : NULL;
    if (e != NULL) {
        if (hold && e->sent) send = false;
        if (send && s_connected && s_client != NULL) e->sent = true;
    }
    bool warn = r != ZC_RETAINED_STORED && !s_retained_warned;
    if (warn) s_retained_warned = true;
    xSemaphoreGive(zc_retained_lock());
    if (warn) {
        ESP_LOGW(TAG, "republish: %s is not re-sent (%s)", topic,
                 r == ZC_RETAINED_FULL ? "more than APP_MQTT_MAX_RETAINED state topics"
                 : r == ZC_RETAINED_TOO_LONG ? "message over 1024 bytes"
                                             : "out of memory");
    }
    return send;
}

/* Never blocks the caller. Same arguments as esp_mqtt_client_publish without
 * the client; len 0 means strlen(data). */
static int zc_mqtt_publish(const char *topic, const char *data, int len, int qos, bool retain)
{
    if (retain && topic != NULL && s_republish_s > 0 && zc_retained_tracked(topic) &&
        !zc_retained_record(topic, data, len, qos)) {
        return 0;
    }
    return zc_mqtt_queue(false, topic, data, len, qos, retain);
}

/* Sleeps in one-second steps, so a changed interval applies at once. Only
 * queues, like every other publisher here. */
static void zc_republish_task(void *arg)
{
    (void)arg;
    uint32_t elapsed = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t every = s_republish_s;
        if (every == 0) {
            elapsed = 0;
            continue;
        }
        if (++elapsed < every) continue;
        elapsed = 0;
        if (!s_connected || s_client == NULL) continue;
        int sent = 0;
        xSemaphoreTake(zc_retained_lock(), portMAX_DELAY);
        for (int i = 0; i < s_retained.n; i++) {
            zc_retained_t *e = &s_retained.e[i];
            if (zc_mqtt_queue(false, e->topic, e->data, e->len, e->qos, true) == 0) {
                e->sent = true;
                sent++;
            }
        }
        xSemaphoreGive(zc_retained_lock());
        ESP_LOGI(TAG, "republished %d state message(s) (every %lu s)", sent, (unsigned long)every);
    }
}

esp_err_t app_mqtt_set_republish_s(uint32_t seconds)
{
    if (seconds > 0 && s_republish_task == NULL &&
        xTaskCreate(zc_republish_task, "mqtt_republish", 3072, NULL, 4, &s_republish_task) != pdPASS) {
        s_republish_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_republish_s = seconds;
    if (seconds == 0) {
        xSemaphoreTake(zc_retained_lock(), portMAX_DELAY);
        zc_retained_clear(&s_retained);
        xSemaphoreGive(zc_retained_lock());
    }
    return ESP_OK;
}

void app_mqtt_set_republish_only_on_schedule(bool only)
{
    s_republish_only = only;
}

static int zc_mqtt_subscribe(const char *topic, int qos)
{
    return zc_mqtt_queue(true, topic, NULL, 0, qos, false);
}

/* Messages queued or being sent; the client's outbox holds the rest. */
__attribute__((unused)) static bool zc_mqtt_tx_pending(void)
{
    return __atomic_load_n(&s_tx_pending, __ATOMIC_SEQ_CST) > 0;
}

/* ── Product entities (app_mqtt_entity_*) ─────────────────────────────── */
#ifndef APP_MQTT_MAX_ENTITIES
#define APP_MQTT_MAX_ENTITIES 16
#endif

typedef struct {
    char component[24];
    char object_id[33];
    char *name;
    char *extra;
    char *state;
    app_mqtt_command_cb_t cb;
    void *cb_ctx;
    bool added;
} zc_entity_t;

static zc_entity_t s_entities[APP_MQTT_MAX_ENTITIES];

/* A state set before app_mqtt_entity_add for its id is kept, latest value only,
 * in the entity's own slot (added = false), and becomes the entity's first
 * value when it is added: products take their first sensor reading (the
 * driver bus replays it at registration) before they add their entities. At
 * most this many ids wait like that; a spelling that is never added must not
 * take every slot. */
#define ZC_PENDING_STATES 8

/* object_ids used without app_mqtt_entity_add, each reported once: a product
 * timer setting the state every few minutes must not flood the log. */
#define ZC_UNKNOWN_IDS 8
static char s_unknown_ids[ZC_UNKNOWN_IDS][33];

/* Caller holds the entity lock. True the first time object_id is seen. */
static bool zc_unknown_first(const char *object_id)
{
    for (int i = 0; i < ZC_UNKNOWN_IDS; i++) {
        if (s_unknown_ids[i][0] == 0) {
            strlcpy(s_unknown_ids[i], object_id, sizeof(s_unknown_ids[i]));
            return true;
        }
        if (strncmp(s_unknown_ids[i], object_id, sizeof(s_unknown_ids[i]) - 1) == 0) return false;
    }
    return false;
}

static SemaphoreHandle_t zc_ent_lock(void)
{
    static StaticSemaphore_t buf;
    static SemaphoreHandle_t h = NULL;
    static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    if (h == NULL) {
        taskENTER_CRITICAL(&mux);
        if (h == NULL) h = xSemaphoreCreateMutexStatic(&buf);
        taskEXIT_CRITICAL(&mux);
    }
    return h;
}

static bool zc_id_ok(const char *s, size_t max, bool digits)
{
    if (s == NULL || s[0] == 0 || strlen(s) > max) return false;
    for (const char *p = s; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || *p == '_' || (digits && *p >= '0' && *p <= '9'))) return false;
    }
    return true;
}

static zc_entity_t *zc_entity_find(const char *object_id)
{
    for (int i = 0; i < APP_MQTT_MAX_ENTITIES; i++) {
        if (s_entities[i].object_id[0] && strcmp(s_entities[i].object_id, object_id) == 0) return &s_entities[i];
    }
    return NULL;
}

static zc_entity_t *zc_entity_find_or_new(const char *object_id)
{
    zc_entity_t *e = zc_entity_find(object_id);
    if (e != NULL) return e;
    for (int i = 0; i < APP_MQTT_MAX_ENTITIES; i++) {
        if (s_entities[i].object_id[0] == 0) {
            strlcpy(s_entities[i].object_id, object_id, sizeof(s_entities[i].object_id));
            return &s_entities[i];
        }
    }
    return NULL;
}

static bool zc_entity_commandable(const char *component)
{
    static const char *const kinds[] = {
        "switch", "number", "button", "select", "text", "light", "fan", "cover", "lock", "siren", "valve",
    };
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        if (strcmp(component, kinds[i]) == 0) return true;
    }
    return false;
}

static bool zc_entity_has_state(const char *component)
{
    return strcmp(component, "button") != 0;
}

static void zc_entity_topic(char *buf, size_t len, const char *object_id, const char *leaf)
{
    snprintf(buf, len, "zerocode/%s/product/%s/%s", s_node_id, object_id, leaf);
}

/* Caller holds the entity lock and the client is connected. */
static void zc_entity_announce(const zc_entity_t *e)
{
    char state_topic[96], cmd_topic[96], disc_topic[128];
    zc_entity_topic(state_topic, sizeof(state_topic), e->object_id, "state");
    zc_entity_topic(cmd_topic, sizeof(cmd_topic), e->object_id, "set");
    snprintf(disc_topic, sizeof(disc_topic), "homeassistant/%s/zc_%s_p_%s/config",
             e->component, s_node_id, e->object_id);

    /* The extra members, without surrounding braces or whitespace. */
    const char *x = e->extra ? e->extra : "";
    while (*x == ' ' || *x == '\\n' || *x == '\\t') x++;
    size_t xl = strlen(x);
    while (xl && (x[xl - 1] == ' ' || x[xl - 1] == '\\n' || x[xl - 1] == '\\t')) xl--;
    if (xl >= 2 && x[0] == '{' && x[xl - 1] == '}') {
        x++;
        xl -= 2;
    }

    size_t cap = 640 + 2 * strlen(e->name) + xl;
    char *json = (char *)malloc(cap);
    if (json == NULL) {
        ESP_LOGW(TAG, "out of memory, entity %s not announced", e->object_id);
        return;
    }
    size_t n = (size_t)snprintf(json, cap, "{\\"name\\":\\"");
    for (const char *p = e->name; *p && n + 3 < cap; p++) {
        if (*p == '"' || *p == '\\\\') json[n++] = '\\\\';
        json[n++] = (unsigned char)*p < 0x20 ? ' ' : *p;
    }
    n += (size_t)snprintf(json + n, cap - n,
                          "\\",\\"unique_id\\":\\"zc_%s_p_%s\\",\\"availability_topic\\":\\"%s\\","
                          "\\"device\\":{\\"identifiers\\":[\\"zc_%s\\"],\\"name\\":\\"ZeroCode %s\\",\\"manufacturer\\":\\"ZeroCode AI\\"}",
                          s_node_id, e->object_id, s_status_topic, s_node_id, s_node_id);
    if (zc_entity_has_state(e->component)) {
        n += (size_t)snprintf(json + n, cap - n, ",\\"state_topic\\":\\"%s\\"", state_topic);
    }
    if (zc_entity_commandable(e->component)) {
        n += (size_t)snprintf(json + n, cap - n, ",\\"command_topic\\":\\"%s\\"", cmd_topic);
    }
    if (xl) n += (size_t)snprintf(json + n, cap - n, ",%.*s", (int)xl, x);
    snprintf(json + n, cap - n, "}");
    zc_mqtt_publish(disc_topic, json, 0, 1, true);
    free(json);

    if (zc_entity_commandable(e->component)) zc_mqtt_subscribe(cmd_topic, 1);
    if (e->state != NULL && zc_entity_has_state(e->component)) {
        zc_mqtt_publish(state_topic, e->state, 0, 1, true);
    }
}

/* Caller holds the entity lock. An id still not added once the broker is
 * connected: its kept state and its command callback reach nobody. Once per id. */
static void zc_entity_report_unadded(const zc_entity_t *e)
{
    if (e->state == NULL && e->cb == NULL) return;
    if (!zc_unknown_first(e->object_id)) return;
    if (e->state != NULL) {
        ESP_LOGE(TAG, "app_mqtt_entity_set_state(\\"%s\\"): no entity with that object_id - call "
                      "app_mqtt_entity_add(component, \\"%s\\", name, extra); until then its state is not published",
                 e->object_id, e->object_id);
    } else {
        /* A command callback with no entity: nothing subscribes to its topic. */
        ESP_LOGE(TAG, "app_mqtt_entity_on_command(\\"%s\\"): no entity with that object_id - call "
                      "app_mqtt_entity_add(component, \\"%s\\", name, extra) first; until then no command reaches it",
                 e->object_id, e->object_id);
    }
}

static void zc_entities_on_connected(void)
{
    xSemaphoreTake(zc_ent_lock(), portMAX_DELAY);
    for (int i = 0; i < APP_MQTT_MAX_ENTITIES; i++) {
        zc_entity_t *e = &s_entities[i];
        if (e->added) {
            zc_entity_announce(e);
        } else if (e->object_id[0]) {
            zc_entity_report_unadded(e);
        }
    }
    xSemaphoreGive(zc_ent_lock());
}

/* Caller holds the entity lock. Ids holding a state while not yet added. */
static int zc_entities_pending(void)
{
    int n = 0;
    for (int i = 0; i < APP_MQTT_MAX_ENTITIES; i++) {
        if (s_entities[i].object_id[0] && !s_entities[i].added && s_entities[i].state != NULL) n++;
    }
    return n;
}

static char *zc_strdup(const char *s)
{
    if (s == NULL) return NULL;
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (d != NULL) memcpy(d, s, n);
    return d;
}

esp_err_t app_mqtt_entity_add(const char *component, const char *object_id, const char *name,
                              const char *extra_json)
{
    if (!zc_id_ok(component, 23, false) || !zc_id_ok(object_id, 32, true) || name == NULL || name[0] == 0 ||
        strlen(name) > 64) {
        return ESP_ERR_INVALID_ARG;
    }
    char *n = zc_strdup(name);
    char *x = extra_json && extra_json[0] ? zc_strdup(extra_json) : NULL;
    if (n == NULL || (extra_json && extra_json[0] && x == NULL)) {
        free(n);
        free(x);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(zc_ent_lock(), portMAX_DELAY);
    zc_entity_t *e = zc_entity_find_or_new(object_id);
    if (e == NULL) {
        xSemaphoreGive(zc_ent_lock());
        free(n);
        free(x);
        return ESP_ERR_NO_MEM;
    }
    strlcpy(e->component, component, sizeof(e->component));
    free(e->name);
    free(e->extra);
    e->name = n;
    e->extra = x;
    e->added = true;
    if (s_connected && s_client != NULL) zc_entity_announce(e);
    xSemaphoreGive(zc_ent_lock());
    return ESP_OK;
}

esp_err_t app_mqtt_entity_set_state(const char *object_id, const char *state)
{
    if (!zc_id_ok(object_id, 32, true) || state == NULL) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(zc_ent_lock(), portMAX_DELAY);
    zc_entity_t *e = zc_entity_find(object_id);
    if (e == NULL || !e->added) {
        /* Not added yet: keep the latest value for app_mqtt_entity_add. */
        bool kept = false;
        if ((e != NULL && e->state != NULL) || zc_entities_pending() < ZC_PENDING_STATES) {
            if (e == NULL) e = zc_entity_find_or_new(object_id);
            char *s = e != NULL ? zc_strdup(state) : NULL;
            if (s != NULL) {
                free(e->state);
                e->state = s;
                kept = true;
            }
        }
        if (kept) {
            /* Already connected and still not added: say so now, once. */
            if (s_connected && s_client != NULL) zc_entity_report_unadded(e);
            xSemaphoreGive(zc_ent_lock());
            return ESP_OK;
        }
        bool first = zc_unknown_first(object_id);
        xSemaphoreGive(zc_ent_lock());
        if (first) {
            ESP_LOGE(TAG, "app_mqtt_entity_set_state(\\"%s\\"): no entity with that object_id, and it cannot wait for "
                          "app_mqtt_entity_add (%d ids already wait, or no room) - value dropped; call "
                          "app_mqtt_entity_add(component, \\"%s\\", name, extra) first",
                     object_id, ZC_PENDING_STATES, object_id);
        }
        return ESP_ERR_NOT_FOUND;
    }
    if (e->state != NULL && strcmp(e->state, state) == 0) {
        xSemaphoreGive(zc_ent_lock());
        return ESP_OK;
    }
    char *s = zc_strdup(state);
    if (s == NULL) {
        xSemaphoreGive(zc_ent_lock());
        return ESP_ERR_NO_MEM;
    }
    free(e->state);
    e->state = s;
    if (s_connected && s_client != NULL && zc_entity_has_state(e->component)) {
        char topic[96];
        zc_entity_topic(topic, sizeof(topic), e->object_id, "state");
        zc_mqtt_publish(topic, s, 0, 1, true);
    }
    xSemaphoreGive(zc_ent_lock());
    return ESP_OK;
}

esp_err_t app_mqtt_entity_on_command(const char *object_id, app_mqtt_command_cb_t cb, void *ctx)
{
    if (!zc_id_ok(object_id, 32, true)) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(zc_ent_lock(), portMAX_DELAY);
    zc_entity_t *e = zc_entity_find_or_new(object_id);
    if (e != NULL) {
        e->cb = cb;
        e->cb_ctx = ctx;
    }
    xSemaphoreGive(zc_ent_lock());
    return e != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

/* zerocode/<node>/product/<object_id>/set -> that entity's callback. */
static void zc_entities_dispatch(const char *topic, int tlen, const char *data, int dlen)
{
    char prefix[48];
    int pl = snprintf(prefix, sizeof(prefix), "zerocode/%s/product/", s_node_id);
    if (tlen <= pl + 4 || strncmp(topic, prefix, pl) != 0 || strncmp(topic + tlen - 4, "/set", 4) != 0) return;
    int ol = tlen - pl - 4;
    char oid[33];
    if (ol <= 0 || ol >= (int)sizeof(oid)) return;
    memcpy(oid, topic + pl, ol);
    oid[ol] = 0;

    xSemaphoreTake(zc_ent_lock(), portMAX_DELAY);
    zc_entity_t *e = zc_entity_find(oid);
    app_mqtt_command_cb_t cb = e ? e->cb : NULL;
    void *ctx = e ? e->cb_ctx : NULL;
    xSemaphoreGive(zc_ent_lock());
    if (cb == NULL) return;

    char *payload = (char *)malloc((size_t)dlen + 1);
    if (payload == NULL) return;
    memcpy(payload, data, dlen);
    payload[dlen] = 0;
    cb(oid, payload, ctx);
    free(payload);
}

/* ── Product topics ───────────────────────────────────────────────────
 * The product's own topics on THIS connection: a second client beside this
 * one (same broker, the same default client id from the MAC) made the broker
 * drop each in turn to accept the other, and the connection flapped forever. */
#ifndef APP_MQTT_MAX_TOPIC_SUBS
#define APP_MQTT_MAX_TOPIC_SUBS 8
#endif
typedef struct {
    char *filter;
    int qos;
    app_mqtt_topic_cb_t cb;
    void *ctx;
} zc_topic_sub_t;
static zc_topic_sub_t s_topic_subs[APP_MQTT_MAX_TOPIC_SUBS];

/* MQTT filter match: '+' is one level, a trailing '#' the rest. */
static bool zc_topic_matches(const char *filter, const char *topic, int tlen)
{
    int t = 0;
    for (const char *f = filter; *f; ) {
        if (*f == '#') return true;
        if (*f == '+') {
            while (t < tlen && topic[t] != '/') t++;
            f++;
            continue;
        }
        if (t >= tlen || topic[t] != *f) return false;
        t++; f++;
    }
    return t == tlen;
}

esp_err_t app_mqtt_publish(const char *topic, const char *payload, int len, int qos, bool retain)
{
    if (topic == NULL || topic[0] == 0 || payload == NULL) return ESP_ERR_INVALID_ARG;
    if (!s_connected) return ESP_ERR_INVALID_STATE;
    return zc_mqtt_publish(topic, payload, len, qos, retain) < 0 ? ESP_FAIL : ESP_OK;
}

esp_err_t app_mqtt_subscribe(const char *filter, int qos, app_mqtt_topic_cb_t cb, void *ctx)
{
    if (filter == NULL || filter[0] == 0 || cb == NULL) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(zc_ent_lock(), portMAX_DELAY);
    int slot = -1;
    for (int i = 0; i < APP_MQTT_MAX_TOPIC_SUBS; i++) {
        if (s_topic_subs[i].filter && strcmp(s_topic_subs[i].filter, filter) == 0) { slot = i; break; }
        if (slot < 0 && s_topic_subs[i].filter == NULL) slot = i;
    }
    if (slot < 0) { xSemaphoreGive(zc_ent_lock()); return ESP_ERR_NO_MEM; }
    if (s_topic_subs[slot].filter == NULL) {
        s_topic_subs[slot].filter = strdup(filter);
        if (s_topic_subs[slot].filter == NULL) { xSemaphoreGive(zc_ent_lock()); return ESP_ERR_NO_MEM; }
    }
    s_topic_subs[slot].qos = qos;
    s_topic_subs[slot].cb = cb;
    s_topic_subs[slot].ctx = ctx;
    bool now = s_connected;
    xSemaphoreGive(zc_ent_lock());
    /* Connected: subscribe now. Otherwise the next connect does it. */
    if (now) zc_mqtt_subscribe(filter, qos);
    return ESP_OK;
}

/* Every (re)connect: the broker forgot the subscriptions of a clean session. */
static void zc_topics_on_connected(void)
{
    xSemaphoreTake(zc_ent_lock(), portMAX_DELAY);
    for (int i = 0; i < APP_MQTT_MAX_TOPIC_SUBS; i++) {
        if (s_topic_subs[i].filter) zc_mqtt_subscribe(s_topic_subs[i].filter, s_topic_subs[i].qos);
    }
    xSemaphoreGive(zc_ent_lock());
}

static void zc_topics_dispatch(const char *topic, int tlen, const char *data, int dlen)
{
    for (int i = 0; i < APP_MQTT_MAX_TOPIC_SUBS; i++) {
        xSemaphoreTake(zc_ent_lock(), portMAX_DELAY);
        const char *filter = s_topic_subs[i].filter;
        bool hit = filter && zc_topic_matches(filter, topic, tlen);
        app_mqtt_topic_cb_t cb = hit ? s_topic_subs[i].cb : NULL;
        void *ctx = s_topic_subs[i].ctx;
        xSemaphoreGive(zc_ent_lock());
        if (cb == NULL) continue;
        char *t = (char *)malloc((size_t)tlen + 1);
        char *d = (char *)malloc((size_t)dlen + 1);
        if (t && d) {
            memcpy(t, topic, tlen); t[tlen] = 0;
            memcpy(d, data, dlen); d[dlen] = 0;
            cb(t, d, dlen, ctx);
        }
        free(t); free(d);
    }
}

/* Weak: a product defines its own to publish more on every (re)connect. */
__attribute__((weak)) void app_mqtt_on_connected(void)
{
}

${collectSlot(rendered, 'mqtt_statics')}

/* Connection-state transitions. Behavior blocks hook these via
 * mqtt_event_cases — \`connected\` is in scope (1 = broker session up). */
static void mqtt_on_event(int connected)
{
    (void)connected;
${collectSlot(rendered, 'mqtt_event_cases', 4)}
}

/* Hardware/param bus -> state topics. Publishes only while connected;
 * mqtt_device_create republishes current state on every (re)connect, so
 * nothing is lost while offline. */
static void mqtt_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
    (void)val;
    if (!s_connected || s_client == NULL) return;
    /* Watches: a param several device types react to (a sensor's status
     * shared by its temperature and humidity entities) is an \`if\`, not a
     * \`case\` — two identical case labels would not compile. */
${collectSlot(rendered, 'mqtt_driver_cb_watch', 4)}
    switch (param_id) {
${collectSlot(rendered, 'mqtt_driver_cb_cases', 4)}
    default:
        break;
    }
}

${devFns.join('\n\n')}

/* Command topics -> hardware. Runs on the MQTT task; payloads are not
 * NUL-terminated, so every case gets (topic, tlen, data, dlen). */
static void mqtt_handle_data_cases(const char *topic, int tlen, const char *data, int dlen)
{
    (void)topic; (void)tlen; (void)data; (void)dlen;
${collectSlot(rendered, 'mqtt_command_cases', 4)}
    zc_entities_dispatch(topic, tlen, data, dlen);
    zc_topics_dispatch(topic, tlen, data, dlen);
}

static void mqtt_handle_data(const char *topic, int tlen, const char *data, int dlen)
{
    app_driver_take_refusal(NULL);
    mqtt_handle_data_cases(topic, tlen, data, dlen);
    /* A command the param's guard refused was echoed as done by its case:
     * report the real value after it (app_driver_take_refusal). */
    app_driver_param_id_t zc_refused;
    if (app_driver_take_refusal(&zc_refused) && app_driver_param_seen(zc_refused)) {
        app_driver_param_val_t zc_now = {};
        app_driver_get_param(zc_refused, &zc_now);
        mqtt_driver_cb(zc_refused, zc_now, APP_DRIVER_SOURCE_LOCAL, NULL);
    }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)arg; (void)base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    s_mqtt_task = xTaskGetCurrentTaskHandle();
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            s_connected = true;
            ESP_LOGI(TAG, "MQTT connected");
            zc_mqtt_publish(s_status_topic, "online", 0, 1, true);
${devCalls.join('\n')}
            zc_entities_on_connected();
            zc_topics_on_connected();
            app_mqtt_on_connected();
            mqtt_on_event(1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_connected = false;
            ESP_LOGW(TAG, "MQTT disconnected — esp-mqtt reconnects on its own");
            mqtt_on_event(0);
            break;
        case MQTT_EVENT_DATA:
            mqtt_handle_data(event->topic, event->topic_len, event->data, event->data_len);
            break;
        default:
            break;
    }
}

static void mqtt_start_client(void)
{
    if (s_client != NULL || s_uri[0] == 0) return;
    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = s_uri;
    if (s_user[0]) cfg.credentials.username = s_user;
    if (s_mqtt_pass[0]) cfg.credentials.authentication.password = s_mqtt_pass;
    cfg.session.last_will.topic = s_status_topic;
    cfg.session.last_will.msg = "offline";
    cfg.session.last_will.qos = 1;
    cfg.session.last_will.retain = true;
    s_client = esp_mqtt_client_init(&cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "mqtt client init failed");
        return;
    }
    esp_mqtt_client_register_event(s_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    ESP_LOGI(TAG, "MQTT connecting to %s", s_uri);
}

${sharedNet ? `/* Shared-network mode: the transport (Matter/RainMaker) owns Wi-Fi. We only
 * listen for connectivity and start the client when it appears. BOTH v4 and
 * v6 events: Matter products often run IPv6-only (CONFIG_LWIP_IPV4=n), in
 * which case the broker URI must be a hostname or IPv6 literal. */
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)arg; (void)event_data; (void)base;
    if (event_id == IP_EVENT_STA_GOT_IP || event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "network up (transport-owned) — starting MQTT");
        mqtt_start_client();
    }
}` : `static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)arg; (void)event_data;
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected — retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi got IP");
        mqtt_start_client();
    }
}

static void mqtt_wifi_start(void)
{
    if (s_wifi_started || s_ssid[0] == 0) return;
    wifi_config_t wc = {};
    strlcpy((char *)wc.sta.ssid, s_ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, s_pass, sizeof(wc.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_started = true;
}`}

/* ── console config: the onboarding surface ───────────────────────── */

static void mqtt_load_config(void)
{
    nvs_handle_t h;
    if (nvs_open("zc_mqtt", NVS_READONLY, &h) != ESP_OK) return;
    size_t n;
${sharedNet ? '' : `    n = sizeof(s_ssid);      nvs_get_str(h, "ssid", s_ssid, &n);
    n = sizeof(s_pass);      nvs_get_str(h, "pass", s_pass, &n);
`}    n = sizeof(s_uri);       nvs_get_str(h, "uri", s_uri, &n);
    n = sizeof(s_user);      nvs_get_str(h, "user", s_user, &n);
    n = sizeof(s_mqtt_pass); nvs_get_str(h, "mpass", s_mqtt_pass, &n);
    nvs_close(h);
}

/* Public setters (app_mqtt.h): validate, persist, apply. The console
 * commands below are thin wrappers, and mqtt-setup asks for the same values
 * one at a time — so a product never needs to know the NVS layout, and "the
 * device asks for its settings" is a command it already has. */
esp_err_t app_mqtt_set_wifi(const char *ssid, const char *password)
{
${sharedNet ? `    /* Shared-network mode: the transport (Matter/RainMaker) owns Wi-Fi and
     * onboards it — there is nothing for this block to store. */
    (void)ssid; (void)password;
    return ESP_ERR_NOT_SUPPORTED;` : `    if (ssid == NULL || ssid[0] == 0) return ESP_ERR_INVALID_ARG;
    if (password == NULL) password = "";
    if (strlen(ssid) >= sizeof(s_ssid) || strlen(password) >= sizeof(s_pass)) return ESP_ERR_INVALID_SIZE;
    nvs_handle_t h;
    esp_err_t err = nvs_open("zc_mqtt", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "pass", password);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return err;
    strlcpy(s_ssid, ssid, sizeof(s_ssid));
    strlcpy(s_pass, password, sizeof(s_pass));
    if (s_wifi_started) {
        wifi_config_t wc = {};
        strlcpy((char *)wc.sta.ssid, s_ssid, sizeof(wc.sta.ssid));
        strlcpy((char *)wc.sta.password, s_pass, sizeof(wc.sta.password));
        esp_wifi_set_config(WIFI_IF_STA, &wc);
        esp_wifi_disconnect();
        esp_wifi_connect();
    } else {
        mqtt_wifi_start();
    }
    return ESP_OK;`}
}

esp_err_t app_mqtt_set_broker(const char *uri, const char *username, const char *password)
{
    if (uri == NULL || uri[0] == 0) return ESP_ERR_INVALID_ARG;
    if (username == NULL) username = "";
    if (password == NULL) password = "";
    if (strlen(uri) >= sizeof(s_uri) || strlen(username) >= sizeof(s_user) ||
        strlen(password) >= sizeof(s_mqtt_pass)) {
        return ESP_ERR_INVALID_SIZE;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open("zc_mqtt", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, "uri", uri);
    if (err == ESP_OK) err = nvs_set_str(h, "user", username);
    if (err == ESP_OK) err = nvs_set_str(h, "mpass", password);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return err;
    strlcpy(s_uri, uri, sizeof(s_uri));
    strlcpy(s_user, username, sizeof(s_user));
    strlcpy(s_mqtt_pass, password, sizeof(s_mqtt_pass));
    if (s_client != NULL) {
        /* URI changed under a live client: restart it clean. */
        if (s_client_lock) xSemaphoreTake(s_client_lock, portMAX_DELAY);
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        s_connected = false;
        if (s_client_lock) xSemaphoreGive(s_client_lock);
    }
    mqtt_start_client();
    return ESP_OK;
}

bool app_mqtt_configured(void)
{
    return ${sharedNet ? '' : 's_ssid[0] != 0 && '}s_uri[0] != 0;
}

bool app_mqtt_owns_wifi(void)
{
    return ${sharedNet ? 'false' : 'true'};
}

${sharedNet ? '' : `static int mqtt_wifi_cmd(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: mqtt-wifi <ssid> <password>\\n");
        return 1;
    }
    esp_err_t err = app_mqtt_set_wifi(argv[1], argv[2]);
    if (err != ESP_OK) {
        printf("wifi config not saved: %s\\n", esp_err_to_name(err));
        return 1;
    }
    printf("wifi config saved — connecting\\n");
    return 0;
}
`}static int mqtt_broker_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: mqtt-broker <uri> [username] [password]  (e.g. mqtt://192.168.1.10)\\n");
        return 1;
    }
    esp_err_t err = app_mqtt_set_broker(argv[1], argc > 2 ? argv[2] : "", argc > 3 ? argv[3] : "");
    if (err != ESP_OK) {
        printf("broker config not saved: %s\\n", esp_err_to_name(err));
        return 1;
    }
    printf("broker config saved\\n");
    return 0;
}

/* mqtt-setup: the guided version of the commands above. */
#define MQTT_SETUP_ANSWER_MS 120000   /* per question: a person may go and find a value */

static bool mqtt_setup_ask(const char *prompt, char *buf, size_t len, bool secret)
{
    for (;;) {
        esp_err_t err = app_console_read_line(prompt, buf, len, secret, MQTT_SETUP_ANSWER_MS);
        if (err == ESP_OK) return true;
        if (err == ESP_ERR_INVALID_SIZE) {
            printf("  too long — at most %u characters\\n", (unsigned)(len - 1));
            continue;
        }
        printf("mqtt-setup: no answer — stopped. Values already saved are kept; run mqtt-setup again to finish.\\n");
        return false;
    }
}

static int mqtt_setup_cmd(int argc, char **argv)
{
    (void)argc; (void)argv;
    char a[129], b[66], c[66], prompt[200];

    printf("mqtt-setup — answer each question; Enter keeps the value in [brackets].\\n");
${sharedNet ? `    printf("Wi-Fi: owned by the smart-home transport here — onboard it through that one.\\n");
` : `    snprintf(prompt, sizeof(prompt), "Wi-Fi network (SSID) [%s]: ", s_ssid);
    if (!mqtt_setup_ask(prompt, a, sizeof(s_ssid), false)) return 1;
    bool keep_ssid = a[0] == 0;
    if (keep_ssid && s_ssid[0] == 0) {
        printf("  skipped — no network set yet\\n");
    } else {
        if (!mqtt_setup_ask(keep_ssid ? "Wi-Fi password [keep]: " : "Wi-Fi password: ", b, sizeof(s_pass), true)) return 1;
        if (!(keep_ssid && b[0] == 0)) {
            /* The setter copies into the live config: never pass it the live
             * config itself as the source. */
            if (keep_ssid) strlcpy(a, s_ssid, sizeof(a));
            esp_err_t err = app_mqtt_set_wifi(a, b);
            if (err != ESP_OK) printf("  not saved: %s\\n", esp_err_to_name(err));
            else printf("  wifi config saved — connecting\\n");
        }
    }
`}
    snprintf(prompt, sizeof(prompt), "Broker URI, e.g. mqtt://192.168.1.10 [%s]: ", s_uri);
    if (!mqtt_setup_ask(prompt, a, sizeof(s_uri), false)) return 1;
    if (a[0] == 0 && s_uri[0] == 0) {
        printf("  skipped — no broker set yet\\n");
    } else {
        /* Enter keeps the stored value, "-" clears it: a blank answer cannot
         * mean both, and moving to an anonymous broker has to be possible. */
        snprintf(prompt, sizeof(prompt), "Broker username (Enter keeps, - for none) [%s]: ", s_user);
        if (!mqtt_setup_ask(prompt, b, sizeof(s_user), false)) return 1;
        if (!mqtt_setup_ask("Broker password (Enter keeps, - for none): ", c, sizeof(s_mqtt_pass), true)) return 1;
        if (a[0] || b[0] || c[0]) {
            if (!a[0]) strlcpy(a, s_uri, sizeof(a));
            if (strcmp(b, "-") == 0) b[0] = 0;
            else if (!b[0]) strlcpy(b, s_user, sizeof(b));
            if (strcmp(c, "-") == 0) c[0] = 0;
            else if (!c[0]) strlcpy(c, s_mqtt_pass, sizeof(c));
            esp_err_t err = app_mqtt_set_broker(a, b, c);
            if (err != ESP_OK) printf("  not saved: %s\\n", esp_err_to_name(err));
            else printf("  broker config saved\\n");
        }
    }

    if (app_mqtt_configured()) {
        printf("mqtt-setup: done — connecting to %s.\\n", s_uri);
    } else {
        printf("mqtt-setup: saved, but still missing:%s%s — run mqtt-setup again to finish.\\n",
               ${sharedNet ? '""' : 's_ssid[0] ? "" : " Wi-Fi"'}, s_uri[0] ? "" : " broker");
    }
    return 0;
}

esp_err_t app_mqtt_init(void)
{
    s_handle = app_driver_register_solution("mqtt", mqtt_driver_cb, NULL);

    s_client_lock = xSemaphoreCreateMutex();
    s_tx_q = xQueueCreate(ZC_MQTT_TX_DEPTH, sizeof(zc_mqtt_tx_t *));
    if (s_client_lock == NULL || s_tx_q == NULL ||
        xTaskCreate(zc_mqtt_tx_task, "mqtt_tx", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "MQTT send task not started");
        return ESP_ERR_NO_MEM;
    }

${sharedNet ? `    /* Transport-owned network: create-if-missing is fine either way (the
     * transport usually created the default loop before us; render order is
     * the product author's). */
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(loop_err);
    }
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, wifi_event_handler, NULL));` : `    ESP_ERROR_CHECK(esp_netif_init());
    /* Tolerant: aws_iot (and any behavior) may have created the default loop
     * already, and a second create is ESP_ERR_INVALID_STATE, not a failure. */
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(loop_err);
    }
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));`}

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_node_id, sizeof(s_node_id), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(s_status_topic, sizeof(s_status_topic), "zerocode/%s/status", s_node_id);

    /* Node-level contributions (behaviors) — before anything connects. */
${collectSlot(rendered, 'mqtt_node_init', 4)}

${sharedNet ? '' : `    esp_console_cmd_t wifi_cmd = {};
    wifi_cmd.command = "mqtt-wifi";
    wifi_cmd.help = "Set Wi-Fi credentials: mqtt-wifi <ssid> <password>";
    wifi_cmd.func = &mqtt_wifi_cmd;
    app_console_register_cmd(&wifi_cmd);
`}    esp_console_cmd_t broker_cmd = {};
    broker_cmd.command = "mqtt-broker";
    broker_cmd.help = "Set broker: mqtt-broker <uri> [username] [password]";
    broker_cmd.func = &mqtt_broker_cmd;
    app_console_register_cmd(&broker_cmd);
    esp_console_cmd_t setup_cmd = {};
    setup_cmd.command = "mqtt-setup";
    setup_cmd.help = "Guided setup: asks for ${sharedNet ? '' : 'Wi-Fi, then '}the broker URI and its credentials";
    setup_cmd.func = &mqtt_setup_cmd;
    app_console_register_cmd(&setup_cmd);

    mqtt_load_config();
${sharedNet ? `    if (s_uri[0] == 0) {
        /* Unconfigured is a valid state — never block boot. Wi-Fi comes from
         * the transport's own onboarding (commissioning / provisioning). */
        ESP_LOGW(TAG, "MQTT broker not configured — run: mqtt-setup (guided), or mqtt-broker <uri>");
    }
    /* Reboot-while-provisioned: the transport may already have an IP before
     * we registered the handler. The v4 check is compiled out on IPv6-only
     * Matter builds (CONFIG_LWIP_IPV4=n removes esp_netif_get_ip_info). */
#if CONFIG_LWIP_IPV4
    {
        esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_ip_info_t ip = {};
        if (sta != NULL && esp_netif_get_ip_info(sta, &ip) == ESP_OK && ip.ip.addr != 0) {
            ESP_LOGI(TAG, "network already up — starting MQTT");
            mqtt_start_client();
        }
    }
#endif` : `    if (s_ssid[0] == 0 || s_uri[0] == 0) {
        /* Unconfigured is a valid state — never block boot. */
        ESP_LOGW(TAG, "MQTT not configured — run: mqtt-setup (guided), or mqtt-wifi <ssid> <pass>, then mqtt-broker <uri>");
    }
    mqtt_wifi_start();`}

    ESP_LOGI(TAG, "MQTT initialized (node %s)", s_node_id);
    return ESP_OK;
}
`
}

export const mqttGlue: FrameworkGlue = {
  id: 'mqtt',
  files(ctx) {
    const { rendered, features, hasDeviceTypes } = ctx
    const out: GeneratedFile[] = []
    out.push({ path: 'components/app_mqtt/app_mqtt.cpp', content: genAppMqttCpp(rendered, features) })
    if (hasDeviceTypes) {
      out.push({ path: 'components/app_mqtt/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_mqtt.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__mqtt espressif__json_parser esp_wifi esp_netif esp_event nvs_flash app_driver app_config app_console console
)
` })
    }
    return out
  },
}
