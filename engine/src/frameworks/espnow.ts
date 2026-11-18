// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { RenderedInstance, ValueType } from '../types.js'
import { collectSlot, collectParamIds } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

/**
 * app_espnow.cpp — the device-to-device framework.
 *
 * Binding: "what does this device say to its peers." Broadcaster device
 * types map param changes to zc_espnow_send(name, type, value) via
 * espnow_emit_cases; listeners match inbound name-hashes to their local
 * params via espnow_recv_cases. Names are the kit vocabulary — the wire
 * carries an FNV-1a hash of the param NAME, so separately built products
 * interoperate without coordinating numeric enums.
 *
 * v1 trust model: open broadcast + group-id filter, channel 1, no
 * encryption (ESP-NOW cannot encrypt broadcast frames). Trusted spaces
 * only; pairing + per-peer LMK is v2.
 */
/** Wire type codes (the shared tag vocabulary). */
const ESPNOW_WIRE_TYPE: Record<ValueType, number> = { bool: 0, u8: 1, i16: 2, u16: 3, u32: 4 }

/**
 * After the listener device types' own cases: a param this product itself
 * sends over ESP-NOW (a composed block's zc_espnow_send("<param>", …)), or one
 * the product declares (template.yaml params:, which product code sends),
 * that arrives by name lands on the bus as an EVENT (so a second identical
 * ring still rings), with espnow's own handle as the source so it is never
 * sent back out. Without it a product's own param, such as a ring, would be
 * sent by one board and dropped by the other. Every other param — a relay's
 * power, a driver's reading — is never taken from the air: frames are open
 * broadcast, so any nearby board could otherwise drive the outputs.
 * Framework-owned params (owned_params) never come from a peer.
 */
function espnowRecvFallback(rendered: RenderedInstance[], productParams: Record<string, string> = {}): string {
  const owned = new Set(rendered.flatMap(r => Object.keys(r.block.owned_params ?? {})))
  const sent = new Set<string>(Object.keys(productParams))
  for (const r of rendered) {
    for (const body of Object.values(r.slots)) {
      for (const m of body.matchAll(/\bzc_espnow_send\s*\(\s*"(APP_DRIVER_PARAM_\w+)"/g)) sent.add(m[1]!)
    }
  }
  const params = collectParamIds(rendered, productParams).filter(p => sent.has(p.name) && !owned.has(p.name))
  if (params.length === 0) return ''
  const rows = params.map(p => `        { "${p.name}", ${p.name}, ${ESPNOW_WIRE_TYPE[p.type]} },`).join('\n')
  const member = (t: number) => ({ 0: 'v.b = (msg.value != 0)', 1: 'v.u8 = (uint8_t)msg.value', 2: 'v.i16 = (int16_t)msg.value', 3: 'v.u16 = (uint16_t)msg.value', 4: 'v.u32 = msg.value' } as Record<number, string>)[t]
  return `    /* Any other param of this product, by name: as an event, never re-sent. */
    static const struct { const char *name; app_driver_param_id_t id; uint8_t type; } s_now_params[] = {
${rows}
    };
    for (size_t i = 0; i < sizeof(s_now_params) / sizeof(s_now_params[0]); i++) {
        if (s_now_params[i].type != msg.type || hash != zc_now_hash(s_now_params[i].name)) continue;
        app_driver_param_val_t v = {};
        switch (msg.type) {
            case 0: ${member(0)}; break;
            case 1: ${member(1)}; break;
            case 2: ${member(2)}; break;
            case 3: ${member(3)}; break;
            default: ${member(4)}; break;
        }
        app_driver_fire_event(s_now_params[i].id, v, s_handle);
        return;
    }`
}

export function genAppEspnowCpp(rendered: RenderedInstance[], productParams: Record<string, string> = {}): string {
  return `#include "app_espnow.h"
#include "app_driver.h"
#include "app_config.h"

#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_timer.h>
#include <esp_console.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
${collectSlot(rendered, 'espnow_includes')}

static const char *TAG = "app_espnow";

static app_driver_handle_t s_handle = 0;

/* Wire format: 14 packed bytes. Value types: 0 bool, 1 u8, 2 i16, 3 u16, 4 u32.
 * Types >= 0xF0 are control frames (pairing) and never reach the device-type
 * cases. */
#define ZC_NOW_MAGIC 0x5A434E57u /* "ZCNW" */
#define ZC_NOW_GROUP 0           /* group-id filter; per-product cfg is v2 */
#define ZC_NOW_T_PAIR_REQ 0xF0
#define ZC_NOW_T_PAIR_ACK 0xF1
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  group;
    uint8_t  type;
    uint32_t name_hash;
    uint32_t value;
} zc_now_msg_t;

static const uint8_t ZC_NOW_BCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* ── Peers and pairing ────────────────────────────────────────────────────
 * A kit with NO stored peers is the open v1 kit: broadcast out, accept every
 * matching frame in. Once a device holds peers it talks to THEM: sends go
 * unicast to each, frames from any other radio are dropped. Peers are learnt
 * in a pairing window both sides open (a button, a console command): the
 * requester broadcasts PAIR_REQ while its window is open, an acceptor with
 * its window open answers PAIR_ACK and stores the requester, the requester
 * stores the acceptor on the ACK. Stored in NVS, so pairing survives a power
 * cycle. This is the peer side of the framework's v2 story; per-peer
 * encryption (LMK) is the next step and does not change this contract. */
#define ZC_NOW_MAX_PEERS 8
#define ZC_NOW_NVS_NS   "zc_now"
#define ZC_NOW_NVS_KEY  "peers"
/* The peer table is read on the Wi-Fi task (every received frame), by
 * whichever task sends, and by the console; it is written by the pairing
 * task and the console. s_peer_lock guards it; anything slow (NVS, the radio)
 * runs on a copy taken under the lock, never holding it. */
static uint8_t s_peers[ZC_NOW_MAX_PEERS][6];
static uint8_t s_peer_count = 0;
static SemaphoreHandle_t s_peer_lock = NULL;
static volatile bool s_pairing = false;
static esp_timer_handle_t s_pair_timer = NULL;   /* window end */
static esp_timer_handle_t s_pair_beacon = NULL;  /* PAIR_REQ repeat */

/* Pairing frames are handed from the Wi-Fi task to this queue: storing a peer
 * writes flash (nvs_commit) and answers with esp_now_send, and doing that in
 * the receive callback stalls the Wi-Fi task — the receive callback must stay
 * short (IDF's espnow example queues for the same reason). */
typedef struct { uint8_t type; uint8_t mac[6]; } zc_now_pair_evt_t;
static QueueHandle_t s_pair_q = NULL;

static void zc_now_lock(void)   { xSemaphoreTake(s_peer_lock, portMAX_DELAY); }
static void zc_now_unlock(void) { xSemaphoreGive(s_peer_lock); }

/* Copies the table; returns the count. */
static uint8_t zc_now_peers_copy(uint8_t out[ZC_NOW_MAX_PEERS][6])
{
    zc_now_lock();
    uint8_t n = s_peer_count;
    memcpy(out, s_peers, (size_t)n * 6);
    zc_now_unlock();
    return n;
}

static bool zc_now_peer_known_locked(const uint8_t *mac)
{
    for (uint8_t i = 0; i < s_peer_count; i++) {
        if (memcmp(s_peers[i], mac, 6) == 0) return true;
    }
    return false;
}

/* An open kit (no peers) accepts every matching frame; a paired one only its peers'. */
static bool zc_now_accepts(const uint8_t *from)
{
    zc_now_lock();
    bool ok = s_peer_count == 0 || (from != NULL && zc_now_peer_known_locked(from));
    zc_now_unlock();
    return ok;
}

static void zc_now_peers_save(const uint8_t peers[][6], uint8_t count)
{
    nvs_handle_t h;
    if (nvs_open(ZC_NOW_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, ZC_NOW_NVS_KEY, peers, (size_t)count * 6);
    nvs_commit(h);
    nvs_close(h);
}

static void zc_now_peers_load(void)
{
    nvs_handle_t h;
    if (nvs_open(ZC_NOW_NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(s_peers);
    if (nvs_get_blob(h, ZC_NOW_NVS_KEY, s_peers, &len) == ESP_OK) {
        s_peer_count = (uint8_t)(len / 6);
    }
    nvs_close(h);
}

/* The radio needs a peer entry before esp_now_send can unicast to it. */
static void zc_now_radio_add_peer(const uint8_t *mac)
{
    if (esp_now_is_peer_exist(mac)) return;
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK) ESP_LOGW(TAG, "add peer failed: %s", esp_err_to_name(err));
}

/* Runs on the pairing task, never the Wi-Fi task. */
static bool zc_now_peer_store(const uint8_t *mac)
{
    uint8_t copy[ZC_NOW_MAX_PEERS][6];
    zc_now_lock();
    if (zc_now_peer_known_locked(mac)) { zc_now_unlock(); return true; }
    if (s_peer_count >= ZC_NOW_MAX_PEERS) {
        zc_now_unlock();
        ESP_LOGW(TAG, "peer table full (%d) — forget a peer first", ZC_NOW_MAX_PEERS);
        return false;
    }
    zc_now_unlock();
    /* The radio entry goes in before the table entry is visible — a send that
     * sees the new peer must find the radio able to unicast to it — and
     * outside the lock, which the Wi-Fi task takes on every received frame. */
    zc_now_radio_add_peer(mac);
    zc_now_lock();
    if (!zc_now_peer_known_locked(mac) && s_peer_count < ZC_NOW_MAX_PEERS) {
        memcpy(s_peers[s_peer_count++], mac, 6);
    }
    uint8_t n = s_peer_count;
    memcpy(copy, s_peers, (size_t)n * 6);
    zc_now_unlock();
    zc_now_peers_save(copy, n);
    ESP_LOGI(TAG, "paired with %02x:%02x:%02x:%02x:%02x:%02x (%d peer%s)",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], n, n == 1 ? "" : "s");
    return true;
}

static void zc_now_send_raw(const uint8_t *mac, uint8_t type, uint32_t name_hash, uint32_t value)
{
    zc_now_msg_t msg = {};
    msg.magic = ZC_NOW_MAGIC;
    msg.group = ZC_NOW_GROUP;
    msg.type = type;
    msg.name_hash = name_hash;
    msg.value = value;
    esp_err_t err = esp_now_send(mac, (const uint8_t *)&msg, sizeof(msg));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "send failed: %s", esp_err_to_name(err));
    }
}

static void zc_now_pair_beacon_cb(void *arg)
{
    (void)arg;
    if (!s_pairing) return;
    zc_now_send_raw(ZC_NOW_BCAST, ZC_NOW_T_PAIR_REQ, 0, 0);
}

static void zc_now_pair_end_cb(void *arg)
{
    (void)arg;
    s_pairing = false;
    (void)esp_timer_stop(s_pair_beacon);
    ESP_LOGI(TAG, "pairing window closed (%d peer%s)", s_peer_count, s_peer_count == 1 ? "" : "s");
}

void zc_espnow_pair_start(uint32_t window_ms)
{
    if (window_ms == 0) window_ms = 30000;
    s_pairing = true;
    (void)esp_timer_stop(s_pair_timer);
    (void)esp_timer_stop(s_pair_beacon);
    (void)esp_timer_start_once(s_pair_timer, (uint64_t)window_ms * 1000ULL);
    (void)esp_timer_start_periodic(s_pair_beacon, 500 * 1000ULL);
    ESP_LOGI(TAG, "pairing window open for %lu ms — open it on the other device too",
             (unsigned long)window_ms);
}

bool zc_espnow_pairing(void) { return s_pairing; }
uint8_t zc_espnow_peer_count(void) { return s_peer_count; }

void zc_espnow_forget_peers(void)
{
    uint8_t copy[ZC_NOW_MAX_PEERS][6];
    zc_now_lock();
    uint8_t n = s_peer_count;
    memcpy(copy, s_peers, (size_t)n * 6);
    s_peer_count = 0;
    zc_now_unlock();
    for (uint8_t i = 0; i < n; i++) (void)esp_now_del_peer(copy[i]);
    zc_now_peers_save(copy, 0);
    ESP_LOGI(TAG, "peers forgotten — back to open broadcast");
}

/* Bench commands. Both sides of a kit need their window open at once, so a
 * console on each is the simplest way to pair two boards on a desk. */
static int zc_now_cmd_pair(int argc, char **argv)
{
    uint32_t ms = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) * 1000UL : 30000UL;
    zc_espnow_pair_start(ms);
    return 0;
}
static int zc_now_cmd_peers(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint8_t peers[ZC_NOW_MAX_PEERS][6];
    uint8_t n = zc_now_peers_copy(peers);
    printf("%d peer(s)%s\\n", n, n ? "" : " (open kit: broadcast)");
    for (uint8_t i = 0; i < n; i++) {
        printf("  %02x:%02x:%02x:%02x:%02x:%02x\\n", peers[i][0], peers[i][1], peers[i][2],
               peers[i][3], peers[i][4], peers[i][5]);
    }
    return 0;
}
static int zc_now_cmd_forget(int argc, char **argv)
{
    (void)argc; (void)argv;
    zc_espnow_forget_peers();
    return 0;
}

/* FNV-1a over the param NAME — the cross-product kit vocabulary. */
static uint32_t zc_now_hash(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

${collectSlot(rendered, 'espnow_statics')}

void zc_espnow_send(const char *param_name, uint8_t type, uint32_t value)
{
    uint32_t hash = zc_now_hash(param_name);
    uint8_t peers[ZC_NOW_MAX_PEERS][6];
    uint8_t n = zc_now_peers_copy(peers);   /* this send's snapshot */
    if (n == 0) {
        zc_now_send_raw(ZC_NOW_BCAST, type, hash, value);
        return;
    }
    for (uint8_t i = 0; i < n; i++) {
        zc_now_send_raw(peers[i], type, hash, value);
    }
}

/* Peers' broadcasts -> local param bus. Listener device types own the cases
 * (hash + type in scope). Runs on the Wi-Fi task — set_param is safe (its
 * own critical section), keep cases short. */
static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len)
{
    if (data_len != (int)sizeof(zc_now_msg_t)) return;
    zc_now_msg_t msg;
    memcpy(&msg, data, sizeof(msg));
    if (msg.magic != ZC_NOW_MAGIC || msg.group != ZC_NOW_GROUP) return;
    const uint8_t *from = info ? info->src_addr : NULL;
    if (msg.type >= 0xF0) {
        /* Pairing. Only while our own window is open — a stray request from a
           neighbour's kit is ignored, which is the whole point of pairing.
           Handed to the pairing task: storing writes flash. A full queue drops
           the frame; the requester repeats PAIR_REQ every 500 ms. */
        if (!from || !s_pairing || s_pair_q == NULL) return;
        if (msg.type != ZC_NOW_T_PAIR_REQ && msg.type != ZC_NOW_T_PAIR_ACK) return;
        zc_now_pair_evt_t evt = {};
        evt.type = msg.type;
        memcpy(evt.mac, from, 6);
        (void)xQueueSend(s_pair_q, &evt, 0);
        return;
    }
    if (!zc_now_accepts(from)) return;
    uint32_t hash = msg.name_hash;
    (void)hash;
${collectSlot(rendered, 'espnow_recv_cases', 4)}
${espnowRecvFallback(rendered, productParams)}
}

/* Hardware/param bus -> broadcasts. Broadcaster device types own the cases. */
static void espnow_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
    (void)val;
    switch (param_id) {
${collectSlot(rendered, 'espnow_driver_cb_cases', 4)}
    default:
        break;
    }
}

static void zc_now_pair_task(void *arg)
{
    (void)arg;
    zc_now_pair_evt_t evt;
    for (;;) {
        if (xQueueReceive(s_pair_q, &evt, portMAX_DELAY) != pdTRUE) continue;
        if (!s_pairing) continue;   /* the window closed while it waited */
        if (evt.type == ZC_NOW_T_PAIR_REQ) {
            if (zc_now_peer_store(evt.mac)) zc_now_send_raw(evt.mac, ZC_NOW_T_PAIR_ACK, 0, 0);
        } else {
            (void)zc_now_peer_store(evt.mac);
        }
    }
}

esp_err_t app_espnow_init(void)
{
    s_handle = app_driver_register_solution("espnow", espnow_driver_cb, NULL);

    s_peer_lock = xSemaphoreCreateMutex();
    s_pair_q = xQueueCreate(4, sizeof(zc_now_pair_evt_t));
    if (s_peer_lock == NULL || s_pair_q == NULL ||
        xTaskCreate(zc_now_pair_task, "now_pair", 3072, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "out of memory starting ESP-NOW pairing");
        return ESP_ERR_NO_MEM;
    }

    /* Wi-Fi up in STA mode, NOT connected — ESP-NOW rides the radio alone.
     * All kit members pin channel 1 (see the trust-model note). */
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(loop_err);
    }
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, ZC_NOW_BCAST, sizeof(peer.peer_addr));
    peer.channel = 0; /* current channel */
    peer.ifidx = WIFI_IF_STA;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    /* Peers learnt in an earlier pairing window, and the window's timers. */
    zc_now_lock();
    zc_now_peers_load();
    zc_now_unlock();
    {
        uint8_t peers[ZC_NOW_MAX_PEERS][6];
        uint8_t n = zc_now_peers_copy(peers);
        for (uint8_t i = 0; i < n; i++) zc_now_radio_add_peer(peers[i]);
    }
    const esp_timer_create_args_t end_args = {
        .callback = &zc_now_pair_end_cb, .arg = NULL, .dispatch_method = ESP_TIMER_TASK,
        .name = "now_pair_end", .skip_unhandled_events = true,
    };
    const esp_timer_create_args_t beacon_args = {
        .callback = &zc_now_pair_beacon_cb, .arg = NULL, .dispatch_method = ESP_TIMER_TASK,
        .name = "now_pair_req", .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&end_args, &s_pair_timer));
    ESP_ERROR_CHECK(esp_timer_create(&beacon_args, &s_pair_beacon));
    {
        esp_console_cmd_t c = {};
        c.command = "espnow-pair"; c.help = "espnow-pair [seconds] — open the pairing window (default 30 s)"; c.func = &zc_now_cmd_pair;
        esp_console_cmd_register(&c);
        c = {};
        c.command = "espnow-peers"; c.help = "list paired peers"; c.func = &zc_now_cmd_peers;
        esp_console_cmd_register(&c);
        c = {};
        c.command = "espnow-forget"; c.help = "forget every paired peer (back to open broadcast)"; c.func = &zc_now_cmd_forget;
        esp_console_cmd_register(&c);
    }

    /* Node-level contributions (behaviors). */
${collectSlot(rendered, 'espnow_node_init', 4)}

    ESP_LOGI(TAG, "ESP-NOW initialized (channel 1, group %d, %d paired peer%s)",
             ZC_NOW_GROUP, s_peer_count, s_peer_count == 1 ? "" : "s");
    return ESP_OK;
}
`
}

export const espnowGlue: FrameworkGlue = {
  id: 'espnow',
  files(ctx) {
    const { rendered, hasDeviceTypes, productParams } = ctx
    const out: GeneratedFile[] = []
    out.push({ path: 'components/app_espnow/app_espnow.cpp', content: genAppEspnowCpp(rendered, productParams) })
    if (hasDeviceTypes) {
      out.push({ path: 'components/app_espnow/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_espnow.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_timer console app_driver app_config
)
` })
    }
    return out
  },
}
