// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { Product, RenderedInstance } from '../types.js'
import { collectSlot } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

/**
 * app_ble_mesh.cpp — the BLE Mesh framework.
 *
 * Mirror of genAppZigbeeCpp: the framework owns everything that is the same
 * for every mesh product, device-type and behavior blocks contribute only
 * their own bindings through the ble_mesh_* slot family.
 *
 * The generated node is deliberately opinionated, because each of these was a
 * way a hand-written mesh node silently did not work:
 *
 *  - the BLE host is brought up and SYNCHRONISED before the mesh stack, and
 *    the device UUID's tail is the synchronised address, so two devices never
 *    beacon the same UUID;
 *  - every model — servers and clients alike — is defined WITH a publication
 *    context, so no publish can run on a NULL pub;
 *  - a client message goes out through esp_ble_mesh_generic_client_set_state
 *    with a full message context and an incrementing TID, never as a raw
 *    one-byte publish (a Set without a fresh TID is dropped by the receiver as
 *    a retransmission of the last one);
 *  - the app key index is READ BACK from the client model's own key list at
 *    send time rather than remembered from a bind event, so it survives a
 *    reboot, and an unbound client refuses loudly instead of sending into
 *    nowhere;
 *  - "am I provisioned" is always esp_ble_mesh_node_is_provisioned(), the
 *    stack's own answer restored from settings, never a local flag;
 *  - a factory reset re-enables provisioning from the reset event, so the node
 *    is joinable again without a reboot;
 *  - a TOGGLE is the inverse of the last state heard (a Status) or sent, never
 *    a constant — Generic OnOff has no toggle value — and the clients are
 *    subscribed to the group they publish to, so the lights' Status arrives;
 *  - dimming is a Generic DELTA Set relative to where the lights are, never an
 *    absolute level from the switch's own guess;
 *  - the framework owns every mesh callback registration and hands products
 *    weak C-linkage hooks instead (status heard, provisioned/reset), because a
 *    product registering its own callback silently replaces the framework's
 *    or is replaced by it.
 */
export function genAppBleMeshCpp(rendered: RenderedInstance[]): string {
  return `#include "app_ble_mesh.h"
#include "app_driver.h"
#include "app_config.h"
#include "app_console.h"

#include <esp_log.h>
#include <esp_console.h>
#include <esp_timer.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_provisioning_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>
#include <esp_ble_mesh_config_model_api.h>
#include <esp_ble_mesh_generic_model_api.h>

#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
${collectSlot(rendered, 'ble_mesh_includes')}

static const char *TAG = "app_ble_mesh";

#define ZC_MESH_COUNT(a)   (sizeof(a) / sizeof((a)[0]))
#define ZC_MESH_SEND_TTL   7
#define ZC_MESH_BEARERS    ((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT))

/* NimBLE's persistent-store hook. Declared here rather than included because
 * the header lives in a private store directory; it is plain C. */
extern "C" void ble_store_config_init(void);

static app_driver_handle_t s_handle = 0;
static volatile bool s_mesh_ready = false;
/* NetKey the node sends on. Restored from the provisioning event; the primary
 * subnet is the right answer both before that and after a reboot. */
static uint16_t s_net_idx = ESP_BLE_MESH_KEY_PRIMARY;
static uint8_t s_tid = 0;
/* What zc_ble_mesh_publish_toggle inverts: the last OnOff state this node
 * HEARD (a Status) or SENT, whichever came last. Off until then, so the
 * first toggle after boot turns the lights on. */
static volatile bool s_last_onoff = false;
/* When this node last SENT an OnOff Set. A light answers a Set with a Status
 * ~100 ms later (more than once with publish retransmits), so a Status heard
 * just after a send may describe the PREVIOUS press: letting it overwrite
 * s_last_onoff makes a quick double tap leave the next toggle doing nothing.
 * Statuses inside this window still reach the hook; only the toggle state
 * ignores them. */
#define ZC_MESH_STATUS_HOLDOFF_US  (500LL * 1000)
static volatile int64_t s_last_onoff_send_us = 0;
/* The Delta gesture in progress (zc_ble_mesh_publish_delta). */
#define ZC_MESH_DELTA_IDLE_US  (5LL * 1000 * 1000)   /* spec window is 6 s */
#define ZC_MESH_DELTA_MAX      65535
static bool s_delta_open = false;
static uint8_t s_delta_tid = 0;
static int32_t s_delta_total = 0;
static uint16_t s_delta_dest = 0;
static int64_t s_delta_last_us = 0;
/* Bytes 0-1 are the ZeroCode marker a provisioner matches on; bytes 2-7 are
 * filled with this device's BLE address once the host has synchronised. */
static uint8_t s_dev_uuid[16] = { ZC_MESH_UUID_B0, ZC_MESH_UUID_B1 };
/* Cleared by a behavior (the provisioner) that must NOT beacon for a
 * provisioner of its own. Everything else advertises when unprovisioned. */
static bool s_advertise_unprovisioned = true;

/* ── BLE host ──────────────────────────────────────────────────────────
 * Mesh drives the controller itself, but the host must be up and its
 * identity address settled first — the address is what makes each node's
 * device UUID unique, so init BLOCKS until sync rather than racing it. */
static SemaphoreHandle_t s_host_sync = NULL;
static uint8_t s_bd_addr[6] = { 0 };

static void ble_host_on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset, reason %d", reason);
}

static void ble_host_on_sync(void)
{
    uint8_t own_addr_type = 0;
    if (ble_hs_util_ensure_addr(0) != 0 || ble_hs_id_infer_auto(0, &own_addr_type) != 0) {
        ESP_LOGE(TAG, "BLE host has no usable identity address");
    } else {
        (void)ble_hs_id_copy_addr(own_addr_type, s_bd_addr, NULL);
    }
    if (s_host_sync) {
        xSemaphoreGive(s_host_sync);
    }
}

static void ble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static esp_err_t ble_host_init(void)
{
    s_host_sync = xSemaphoreCreateBinary();
    if (!s_host_sync) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init the BLE host (%s)", esp_err_to_name(err));
        return err;
    }
    ble_hs_cfg.reset_cb = ble_host_on_reset;
    ble_hs_cfg.sync_cb = ble_host_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();
    nimble_port_freertos_init(ble_host_task);
    xSemaphoreTake(s_host_sync, portMAX_DELAY);
    return ESP_OK;
}

/* ── Composition ───────────────────────────────────────────────────────
 * One element carrying, always: the Config Server (how a provisioner
 * configures us), the Config Client (what a provisioner behavior drives),
 * Generic OnOff/Level SERVERS (what a controller writes to us) and Generic
 * OnOff/Level CLIENTS (what we publish with). A product that uses only half
 * of them pays a few hundred bytes; a product that is missing the half it
 * needs does not work at all, and nothing in the build says so. */
/* The mesh model macros initialise the fields THEY care about and leave the
 * rest to zero-initialisation, which is exactly right and which C++ warns
 * about anyway. Scoped to these definitions only, so a real missing
 * initialiser anywhere else in this file still shows up. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static esp_ble_mesh_cfg_srv_t s_config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_ENABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = ZC_MESH_SEND_TTL,
};

static esp_ble_mesh_client_t s_cfg_client;
static esp_ble_mesh_client_t s_onoff_client;
static esp_ble_mesh_client_t s_level_client;

ESP_BLE_MESH_MODEL_PUB_DEFINE(zc_onoff_srv_pub, 2 + 3, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(zc_level_srv_pub, 2 + 5, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(zc_onoff_cli_pub, 2 + 4, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(zc_level_cli_pub, 2 + 7, ROLE_NODE);

static esp_ble_mesh_gen_onoff_srv_t s_onoff_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
};

static esp_ble_mesh_gen_level_srv_t s_level_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
};

static esp_ble_mesh_model_t s_root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&s_config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&s_cfg_client),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&zc_onoff_srv_pub, &s_onoff_server),
    ESP_BLE_MESH_MODEL_GEN_LEVEL_SRV(&zc_level_srv_pub, &s_level_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&zc_onoff_cli_pub, &s_onoff_client),
    ESP_BLE_MESH_MODEL_GEN_LEVEL_CLI(&zc_level_cli_pub, &s_level_client),
};

/* Written out rather than built with ESP_BLE_MESH_ELEMENT: that macro's empty
 * vendor-model list is a C compound literal, which this translation unit
 * (C++, because it calls the driver bus) cannot use. */
static esp_ble_mesh_elem_t s_elements[] = {
    {
        .element_addr = ESP_BLE_MESH_ADDR_UNASSIGNED,
        .location = 0,
        .sig_model_count = (uint8_t)ZC_MESH_COUNT(s_root_models),
        .vnd_model_count = 0,
        .sig_models = s_root_models,
        .vnd_models = NULL,
    },
};

static esp_ble_mesh_comp_t s_composition = {
    .cid = ZC_MESH_CID,
    .pid = 0,
    .vid = 0,
    .element_count = (uint8_t)ZC_MESH_COUNT(s_elements),
    .elements = s_elements,
};

static esp_ble_mesh_prov_t s_provision = {
    .uuid = s_dev_uuid,
    /* No OOB: the product is provisioned from a phone app or from a kit
     * member running behaviors/ble_mesh_provisioner. */
    .output_size = 0,
    .output_actions = 0,
    .input_size = 0,
    .input_actions = 0,
#if CONFIG_BLE_MESH_PROVISIONER
    /* Only used when a behavior turns this node into the provisioner. */
    .prov_uuid = s_dev_uuid,
    .prov_unicast_addr = 0x0001,
    .prov_start_address = 0x0005,
    .prov_attention = 0x00,
    .prov_algorithm = 0x00,
    .prov_pub_key_oob = 0x00,
    .prov_static_oob_val = NULL,
    .prov_static_oob_len = 0x00,
    .flags = 0x00,
    .iv_index = 0x00,
#endif
};

#pragma GCC diagnostic pop

${collectSlot(rendered, 'ble_mesh_statics')}

/* ── Level scaling ─────────────────────────────────────────────────────
 * ONE definition, used in both directions, so a driver level that goes out
 * onto the mesh and comes back lands on the value it started from. */
int16_t zc_ble_mesh_level_from_u8(uint8_t value)
{
    /* Rounded both ways: truncating in each direction took most values down a
     * step per round trip (1 -> 0, 100 -> 99), so an echoed brightness crept
     * down. The driver range is 0-254; 255 would overflow int16 (33025 wraps
     * to -32511, nearly off), so it is clamped. */
    if (value > 254) value = 254;
    return (int16_t)(((int32_t)value * 65535 + 127) / 254 - 32768);
}

uint8_t zc_ble_mesh_level_to_u8(int16_t level)
{
    return (uint8_t)((((int32_t)level + 32768) * 254 + 32767) / 65535);
}

/* ── Product hooks ─────────────────────────────────────────────────────
 * Weak do-nothing defaults; a product defines the same signature anywhere it
 * includes app_ble_mesh.h and its definition wins. The header declares them
 * inside extern "C", and so are these: a default with one linkage and an
 * override with the other are two different symbols, and the override would
 * link and never run. */
extern "C" __attribute__((weak)) void zc_ble_mesh_on_onoff_status(uint16_t src, bool on)
{
    (void)src;
    (void)on;
}

extern "C" __attribute__((weak)) void zc_ble_mesh_on_level_status(uint16_t src, int16_t level)
{
    (void)src;
    (void)level;
}

extern "C" __attribute__((weak)) void zc_ble_mesh_on_provisioned(bool provisioned)
{
    (void)provisioned;
}

bool zc_ble_mesh_provisioned(void)
{
    return esp_ble_mesh_node_is_provisioned();
}

/* The app key a client model may send with. Read from the model's own key
 * list every time, because that list is what the stack restores from settings
 * — a value cached at bind time is gone after the first reboot, and sending
 * with ESP_BLE_MESH_KEY_UNUSED fails silently. */
static uint16_t client_app_idx(esp_ble_mesh_client_t *client)
{
    if (!client || !client->model) {
        return ESP_BLE_MESH_KEY_UNUSED;
    }
    for (size_t i = 0; i < ZC_MESH_COUNT(client->model->keys); i++) {
        if (client->model->keys[i] != ESP_BLE_MESH_KEY_UNUSED) {
            return client->model->keys[i];
        }
    }
    return ESP_BLE_MESH_KEY_UNUSED;
}

/* Where a client command goes. A provisioner that puts this node's client in a
 * group (Config Model Publication Set — the room picked in the phone app) means
 * \"send there\", so a command to the default group follows the model's
 * configured publication address; 0xC000 is only the zero-configuration
 * default. Any other address the caller names is kept as given. */
static uint16_t client_dest(esp_ble_mesh_client_t *client, uint16_t addr)
{
    if (addr == ZC_MESH_GROUP_ADDR && client && client->model && client->model->pub &&
        client->model->pub->publish_addr != ESP_BLE_MESH_ADDR_UNASSIGNED) {
        return client->model->pub->publish_addr;
    }
    return addr;
}

/* A client hears a Status only on an address one of its models is subscribed
 * to, and nothing subscribes a CLIENT by default (a provisioner subscribes the
 * servers). Subscribe it to the group it publishes to, which is where the
 * lights publish their status, so zc_ble_mesh_publish_toggle has something to
 * invert and the status hooks fire. Idempotent and cheap: the model's own
 * subscription list is checked first, and the stack stores it in settings. */
static void client_listen(esp_ble_mesh_client_t *client, uint16_t group)
{
    if (!client || !client->model || !ESP_BLE_MESH_ADDR_IS_GROUP(group)) {
        return;
    }
    for (size_t i = 0; i < ZC_MESH_COUNT(client->model->groups); i++) {
        if (client->model->groups[i] == group) {
            return;
        }
    }
    uint16_t elem = s_elements[0].element_addr;
    if (!ESP_BLE_MESH_ADDR_IS_UNICAST(elem)) {
        return;   /* not addressed yet — retried on the next send */
    }
    esp_err_t err = esp_ble_mesh_model_subscribe_group_addr(
        elem, ESP_BLE_MESH_CID_NVAL, client->model->model_id, group);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "could not subscribe client 0x%04x to 0x%04x (%s) — "
                      "status from the lights will not be heard",
                 client->model->model_id, group, esp_err_to_name(err));
    }
}

static void clients_listen(void)
{
    client_listen(&s_onoff_client, client_dest(&s_onoff_client, ZC_MESH_GROUP_ADDR));
    client_listen(&s_level_client, client_dest(&s_level_client, ZC_MESH_GROUP_ADDR));
}

static esp_err_t client_msg_common(esp_ble_mesh_client_common_param_t *common,
                                   esp_ble_mesh_client_t *client,
                                   uint32_t opcode, uint16_t addr)
{
    uint16_t app_idx = client_app_idx(client);
    if (app_idx == ESP_BLE_MESH_KEY_UNUSED) {
        ESP_LOGW(TAG, "no application key bound to this client model yet — "
                      "provision the node and let the provisioner bind it before sending");
        return ESP_ERR_INVALID_STATE;
    }
    common->opcode = opcode;
    common->model = client->model;
    common->ctx.net_idx = s_net_idx;
    common->ctx.app_idx = app_idx;
    common->ctx.addr = addr;
    common->ctx.send_ttl = ZC_MESH_SEND_TTL;
    common->msg_timeout = 0;    /* 0 = the configured default */
    return ESP_OK;
}

esp_err_t zc_ble_mesh_publish_onoff(uint16_t addr, bool on)
{
    if (!s_mesh_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    addr = client_dest(&s_onoff_client, addr);
    esp_ble_mesh_client_common_param_t common = {};
    esp_err_t err = client_msg_common(&common, &s_onoff_client,
                                      ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK, addr);
    if (err != ESP_OK) {
        return err;
    }
    esp_ble_mesh_generic_client_set_state_t set = {};
    set.onoff_set.op_en = false;
    set.onoff_set.onoff = on ? 1 : 0;
    /* A fresh transaction id per message: a receiver discards a Set whose TID
     * repeats the last one from the same source as a retransmission, so a
     * fixed TID makes every command after the first one disappear. */
    set.onoff_set.tid = s_tid++;
    err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "OnOff Set to 0x%04x failed (%s)", addr, esp_err_to_name(err));
    } else {
        s_last_onoff = on;
        s_last_onoff_send_us = esp_timer_get_time();
        client_listen(&s_onoff_client, addr);
        ESP_LOGI(TAG, "published OnOff %s to 0x%04x", on ? "ON" : "OFF", addr);
    }
    return err;
}

esp_err_t zc_ble_mesh_publish_toggle(uint16_t addr)
{
    /* Generic OnOff has no toggle value: send the inverse of what the lights
     * last said (or what we last told them), off before either. */
    return zc_ble_mesh_publish_onoff(addr, !s_last_onoff);
}

esp_err_t zc_ble_mesh_publish_level(uint16_t addr, int16_t level)
{
    if (!s_mesh_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    addr = client_dest(&s_level_client, addr);
    esp_ble_mesh_client_common_param_t common = {};
    esp_err_t err = client_msg_common(&common, &s_level_client,
                                      ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET_UNACK, addr);
    if (err != ESP_OK) {
        return err;
    }
    esp_ble_mesh_generic_client_set_state_t set = {};
    set.level_set.op_en = false;
    set.level_set.level = level;
    set.level_set.tid = s_tid++;
    err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Level Set to 0x%04x failed (%s)", addr, esp_err_to_name(err));
    } else {
        client_listen(&s_level_client, addr);
        ESP_LOGI(TAG, "published Level %d to 0x%04x", (int)level, addr);
    }
    return err;
}

static int32_t delta_clamp(int64_t v)
{
    if (v > ZC_MESH_DELTA_MAX) return ZC_MESH_DELTA_MAX;
    if (v < -ZC_MESH_DELTA_MAX) return -ZC_MESH_DELTA_MAX;
    return (int32_t)v;
}

/* Generic Delta Set Unacknowledged. \`delta\` is the caller's per-call step.
 *
 * The spec's transaction: Delta Sets sharing a TID (same source and
 * destination, within 6 s) are each relative to the level at the START of the
 * transaction, so a held gesture re-sends the running total under one TID.
 * ESP-IDF's Generic Level server only tracks that for a UNICAST destination
 * (bt_mesh_is_server_recv_last_msg); for a group it treats every message as a
 * new transaction and adds the delta to the CURRENT level — so a running
 * total sent to a group accelerates. To a group, therefore, each step goes
 * out alone with a fresh TID, which is correct on every server. */
esp_err_t zc_ble_mesh_publish_delta(uint16_t addr, int32_t delta, bool continue_hold)
{
    if (!s_mesh_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    addr = client_dest(&s_level_client, addr);
    esp_ble_mesh_client_common_param_t common = {};
    esp_err_t err = client_msg_common(&common, &s_level_client,
                                      ESP_BLE_MESH_MODEL_OP_GEN_DELTA_SET_UNACK, addr);
    if (err != ESP_OK) {
        return err;
    }
    int64_t now = esp_timer_get_time();
    bool unicast = ESP_BLE_MESH_ADDR_IS_UNICAST(addr);
    bool same = continue_hold && unicast && s_delta_open && s_delta_dest == addr &&
                (now - s_delta_last_us) < ZC_MESH_DELTA_IDLE_US;
    int32_t send;
    if (same) {
        s_delta_total = delta_clamp((int64_t)s_delta_total + delta);
        send = s_delta_total;
    } else {
        s_delta_tid = s_tid++;
        s_delta_total = delta_clamp(delta);
        send = s_delta_total;
    }
    s_delta_open = true;
    s_delta_dest = addr;
    s_delta_last_us = now;

    esp_ble_mesh_generic_client_set_state_t set = {};
    set.delta_set.op_en = false;
    set.delta_set.level = send;
    set.delta_set.tid = s_delta_tid;
    err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Delta Set to 0x%04x failed (%s)", addr, esp_err_to_name(err));
    } else {
        client_listen(&s_level_client, addr);
        ESP_LOGI(TAG, "published Delta %" PRId32 " (tid %u%s) to 0x%04x",
                 send, (unsigned)s_delta_tid, same ? ", same gesture" : "", addr);
    }
    return err;
}

/* A server model may publish its status only once somebody has configured a
 * publish address for it. Asking the model itself is the honest test — a
 * \"am I provisioned\" check gets this wrong in both directions (a provisioner
 * is not itself a provisioned node, and a provisioned node's server may still
 * have no publication set). */
static bool server_can_publish(esp_ble_mesh_model_t *model)
{
    return model && model->pub && model->pub->publish_addr != ESP_BLE_MESH_ADDR_UNASSIGNED;
}

esp_err_t zc_ble_mesh_set_onoff_state(bool on)
{
    s_onoff_server.state.onoff = on ? 1 : 0;
    s_onoff_server.state.target_onoff = on ? 1 : 0;
    if (!s_mesh_ready || !server_can_publish(s_onoff_server.model)) {
        return ESP_OK;   /* state kept; nobody has asked to be told yet */
    }
    return esp_ble_mesh_model_publish(s_onoff_server.model,
                                      ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                      sizeof(s_onoff_server.state.onoff),
                                      &s_onoff_server.state.onoff, ROLE_NODE);
}

esp_err_t zc_ble_mesh_set_level_state(int16_t level)
{
    s_level_server.state.level = level;
    s_level_server.state.target_level = level;
    if (!s_mesh_ready || !server_can_publish(s_level_server.model)) {
        return ESP_OK;
    }
    return esp_ble_mesh_model_publish(s_level_server.model,
                                      ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_STATUS,
                                      sizeof(s_level_server.state.level),
                                      (uint8_t *)&s_level_server.state.level, ROLE_NODE);
}

esp_err_t zc_ble_mesh_join_mode(void)
{
    if (zc_ble_mesh_provisioned()) {
        ESP_LOGW(TAG, "already provisioned (address 0x%04x) — run \\"mesh reset\\" first",
                 esp_ble_mesh_get_primary_element_address());
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_ble_mesh_node_prov_enable(ZC_MESH_BEARERS);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "advertising as unprovisioned — open your mesh app to add this device");
    } else {
        ESP_LOGW(TAG, "could not start advertising (%s)", esp_err_to_name(err));
    }
    return err;
}

esp_err_t zc_ble_mesh_factory_reset(void)
{
    /* Resets the node AND erases its stored mesh settings. The reset event
     * (handled below) puts it straight back into unprovisioned advertising,
     * so this is recoverable without a reboot. */
    esp_err_t err = esp_ble_mesh_node_local_reset();
    ESP_LOGW(TAG, "left the mesh network (%s)", esp_err_to_name(err));
    return err;
}

/* ── Mesh → hardware ───────────────────────────────────────────────────
 * Device types translate an incoming Generic state into their own driver
 * params here. \`onoff\` / \`level\` are in scope; the driver bus takes care of
 * telling every OTHER solution. */
static void apply_onoff_from_mesh(uint8_t onoff)
{
    (void)onoff;
${collectSlot(rendered, 'ble_mesh_onoff_set_cases', 4)}
}

static void apply_level_from_mesh(int16_t level)
{
    (void)level;
${collectSlot(rendered, 'ble_mesh_level_set_cases', 4)}
}

static void generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                              esp_ble_mesh_generic_server_cb_param_t *param)
{
    if (!param || event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT) {
        return;
    }
    switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:
            apply_onoff_from_mesh(param->value.state_change.onoff_set.onoff);
            break;
        case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET_UNACK:
            apply_level_from_mesh(param->value.state_change.level_set.level);
            break;
        case ESP_BLE_MESH_MODEL_OP_GEN_DELTA_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_DELTA_SET_UNACK:
            apply_level_from_mesh(param->value.state_change.delta_set.level);
            break;
        case ESP_BLE_MESH_MODEL_OP_GEN_MOVE_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_MOVE_SET_UNACK:
            apply_level_from_mesh(param->value.state_change.move_set.level);
            break;
        default:
            break;
    }
}

static void generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                              esp_ble_mesh_generic_client_cb_param_t *param)
{
    if (!param || !param->params) {
        return;
    }
    if (event == ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT) {
        ESP_LOGW(TAG, "no response to opcode 0x%04" PRIx32 " from 0x%04x",
                 param->params->opcode, param->params->ctx.addr);
        return;
    }
    /* A Status arrives as PUBLISH (unsolicited — a light publishing its state,
     * opcode = the Status) or as the answer to our own Get/Set (opcode = the
     * request). Either way it is the lights' state: remember it for the
     * toggle and tell the product. */
    if (param->error_code == 0 &&
        (event == ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT ||
         event == ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT ||
         event == ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT)) {
        uint16_t src = param->params->ctx.addr;
        switch (param->params->opcode) {
            case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS:
            case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
            case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET: {
                const esp_ble_mesh_gen_onoff_status_cb_t *st = &param->status_cb.onoff_status;
                bool on = (st->op_en ? st->target_onoff : st->present_onoff) != 0;
                if (esp_timer_get_time() - s_last_onoff_send_us > ZC_MESH_STATUS_HOLDOFF_US) {
                    s_last_onoff = on;
                }
                ESP_LOGD(TAG, "OnOff status from 0x%04x: %s", src, on ? "ON" : "OFF");
                /* Controller device types mirror what the lights report onto
                 * their own params (written as this framework, so never sent
                 * back out): product code reads the lights' state there. */
${collectSlot(rendered, 'ble_mesh_onoff_status', 16)}
                zc_ble_mesh_on_onoff_status(src, on);
                return;
            }
            case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_STATUS:
            case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_GET:
            case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET:
            case ESP_BLE_MESH_MODEL_OP_GEN_DELTA_SET:
            case ESP_BLE_MESH_MODEL_OP_GEN_MOVE_SET: {
                const esp_ble_mesh_gen_level_status_cb_t *st = &param->status_cb.level_status;
                int16_t level = st->op_en ? st->target_level : st->present_level;
                ESP_LOGD(TAG, "Level status from 0x%04x: %d", src, (int)level);
${collectSlot(rendered, 'ble_mesh_level_status', 16)}
                zc_ble_mesh_on_level_status(src, level);
                return;
            }
            default:
                break;
        }
    }
    ESP_LOGD(TAG, "generic client event %u, opcode 0x%04" PRIx32 ", err %d",
             event, param->params->opcode, param->error_code);
}

/* ── Hardware → mesh ───────────────────────────────────────────────────
 * Runs OFF the mesh task whenever any other solution or the hardware itself
 * changes a param. Device types either update their server state (an
 * actuator reporting itself) or publish through a client (a controller). */
static void ble_mesh_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)param_id;
    (void)val;
    (void)source;
    (void)ctx;
    if (!s_mesh_ready) return;
${collectSlot(rendered, 'ble_mesh_driver_cb_cases', 4)}
}

static void config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                             esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (!param || event != ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        return;
    }
    switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ESP_LOGI(TAG, "application key added (net_idx 0x%04x, app_idx 0x%04x)",
                     param->value.state_change.appkey_add.net_idx,
                     param->value.state_change.appkey_add.app_idx);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "model 0x%04x bound to app_idx 0x%04x",
                     param->value.state_change.mod_app_bind.model_id,
                     param->value.state_change.mod_app_bind.app_idx);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
            ESP_LOGI(TAG, "model 0x%04x subscribed to 0x%04x",
                     param->value.state_change.mod_sub_add.model_id,
                     param->value.state_change.mod_sub_add.sub_addr);
            break;
        default:
            break;
    }
}

/* Provisioning lifecycle. Behaviors contribute ble_mesh_prov_cases as plain
 * if-statements on \`event\` (with \`param\` in scope) — a provisioner block
 * handles its own PROVISIONER_* events there. */
static void provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                            esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
        case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
            ESP_LOGI(TAG, "waiting to be provisioned");
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
            ESP_LOGI(TAG, "provisioning link open (%s)",
                     param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
            ESP_LOGI(TAG, "provisioning link closed (%s)",
                     param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
            s_net_idx = param->node_prov_complete.net_idx;
            ESP_LOGI(TAG, "provisioned: address 0x%04x, net_idx 0x%04x",
                     param->node_prov_complete.addr, param->node_prov_complete.net_idx);
            clients_listen();
            zc_ble_mesh_on_provisioned(true);
            break;
        case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
            /* Settings are erased by now. Go straight back to advertising so
             * the node can be re-added without a power cycle. */
            s_net_idx = ESP_BLE_MESH_KEY_PRIMARY;
            s_last_onoff = false;
            s_delta_open = false;
            if (s_advertise_unprovisioned) {
                (void)esp_ble_mesh_node_prov_enable(ZC_MESH_BEARERS);
            }
            ESP_LOGW(TAG, "node reset — advertising as unprovisioned again");
            zc_ble_mesh_on_provisioned(false);
            break;
        default:
            break;
    }
${collectSlot(rendered, 'ble_mesh_prov_cases', 4)}
}

/* ── Console ───────────────────────────────────────────────────────────
 * One command, because the questions a mesh device raises on a bench are
 * always the same five: is it in the network, get it in, get it out, and
 * make it say something. */
static int mesh_console_cmd(int argc, char **argv)
{
    const char *sub = (argc > 1) ? argv[1] : "status";
    if (!strcmp(sub, "status")) {
        bool joined = zc_ble_mesh_provisioned();
        printf("provisioned : %s\\n", joined ? "yes" : "no");
        printf("address     : 0x%04x\\n", esp_ble_mesh_get_primary_element_address());
        printf("net_idx     : 0x%04x\\n", s_net_idx);
        printf("onoff cli   : app_idx 0x%04x\\n", client_app_idx(&s_onoff_client));
        printf("level cli   : app_idx 0x%04x\\n", client_app_idx(&s_level_client));
        printf("group       : 0x%04x\\n", ZC_MESH_GROUP_ADDR);
        return 0;
    }
    if (!strcmp(sub, "join")) {
        return zc_ble_mesh_join_mode() == ESP_OK ? 0 : 1;
    }
    if (!strcmp(sub, "reset")) {
        return zc_ble_mesh_factory_reset() == ESP_OK ? 0 : 1;
    }
    if (!strcmp(sub, "on") || !strcmp(sub, "off")) {
        return zc_ble_mesh_publish_onoff(ZC_MESH_GROUP_ADDR, !strcmp(sub, "on")) == ESP_OK ? 0 : 1;
    }
    if (!strcmp(sub, "toggle")) {
        return zc_ble_mesh_publish_toggle(ZC_MESH_GROUP_ADDR) == ESP_OK ? 0 : 1;
    }
    printf("usage: mesh <status|join|reset|on|off|toggle>\\n");
    return 1;
}

esp_err_t app_ble_mesh_init(void)
{
    s_handle = app_driver_register_solution("ble_mesh", ble_mesh_driver_cb, NULL);

    esp_console_cmd_t mesh_cmd = {};
    mesh_cmd.command = "mesh";
    mesh_cmd.help = "BLE Mesh: mesh <status|join|reset|on|off|toggle>";
    mesh_cmd.func = &mesh_console_cmd;
    app_console_register_cmd(&mesh_cmd);

    esp_err_t err = ble_host_init();
    if (err != ESP_OK) {
        return err;
    }
    /* Marker stays in bytes 0-1; the rest is this device's own address, so no
     * two devices beacon the same UUID. */
    memcpy(s_dev_uuid + 2, s_bd_addr, sizeof(s_bd_addr));

    esp_ble_mesh_register_prov_callback(provisioning_cb);
    esp_ble_mesh_register_config_server_callback(config_server_cb);
    esp_ble_mesh_register_generic_server_callback(generic_server_cb);
    esp_ble_mesh_register_generic_client_callback(generic_client_cb);

    err = esp_ble_mesh_init(&s_provision, &s_composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize the mesh stack (%s)", esp_err_to_name(err));
        return err;
    }
    s_mesh_ready = true;

    /* Node-level features contributed by behavior blocks (the provisioner,
     * …) and each device type's initial state. */
${collectSlot(rendered, 'ble_mesh_node_init', 4)}

    if (zc_ble_mesh_provisioned()) {
        ESP_LOGI(TAG, "BLE Mesh node ready — already provisioned, address 0x%04x",
                 esp_ble_mesh_get_primary_element_address());
        clients_listen();
        zc_ble_mesh_on_provisioned(true);
    } else if (s_advertise_unprovisioned) {
        err = esp_ble_mesh_node_prov_enable(ZC_MESH_BEARERS);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to advertise as unprovisioned (%s)", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "BLE Mesh node ready — unprovisioned, waiting to be added");
    } else {
        ESP_LOGI(TAG, "BLE Mesh ready — this device provisions the network");
    }
    return ESP_OK;
}
`
}

export const bleMeshGlue: FrameworkGlue = {
  id: 'ble_mesh',
  files(ctx) {
    const { rendered, hasDeviceTypes } = ctx
    const out: GeneratedFile[] = []
    out.push({ path: 'components/app_ble_mesh/app_ble_mesh.cpp', content: genAppBleMeshCpp(rendered) })
    if (hasDeviceTypes) {
      out.push({ path: 'components/app_ble_mesh/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_ble_mesh.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES bt nvs_flash app_driver app_config app_console console esp_timer
)
` })
    }
    return out
  },
}
