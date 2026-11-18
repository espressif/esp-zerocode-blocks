// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { RenderedInstance } from '../types.js'
import { type CatalogFeatures, isFramework, collectSlot, CURRENT_CATALOG, refusalShim, indentText, unwrapInitBody } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

/** frameworks/zigbee's own param: true while the device is on a Zigbee network
 *  (joined, or rebooted onto the network it had joined), false while it looks
 *  for one. Emitted only when the framework block declares it (owned_params). */
const ZIGBEE_JOINED_PARAM = 'APP_DRIVER_PARAM_ZIGBEE_JOINED'

export function genAppZigbeeCpp(rendered: RenderedInstance[], features: CatalogFeatures = CURRENT_CATALOG): string {
  const joined = rendered.some(r => isFramework(r, 'zigbee') && ZIGBEE_JOINED_PARAM in (r.block.owned_params ?? {}))
  const pub = (v: boolean, pad: string) => joined ? `\n${pad}zc_zigbee_publish_joined(${v});` : ''
  // Mirror of genAppMatterCpp/genAppRainmakerCpp: each device-type instance's
  // zigbee_endpoint_create slot becomes its own static init function that adds
  // its endpoint to the shared device descriptor, called in sequence from the
  // Zigbee task.
  const usedEp = new Set<string>()
  const epFns: string[] = []
  const epCalls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['zigbee_endpoint_create'] ?? '').trim()) continue
    const slug = (r.prefix || r.block.id.split('/').pop() || 'ep').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; usedEp.has(name); n++) name = `${slug}_${n}`
    usedEp.add(name)
    epFns.push(`static esp_err_t zc_${name}_zigbee_endpoint_init(ezb_af_device_desc_t dev_desc)\n{\n${indentText(unwrapInitBody(r.slots['zigbee_endpoint_create']), 4)}\n    return ESP_OK;\n}`)
    epCalls.push(`    if (zc_${name}_zigbee_endpoint_init(dev_desc) != ESP_OK) {\n        ESP_LOGE(TAG, "endpoint init failed");\n        vTaskDelete(NULL);\n        return;\n    }`)
  }
  return `#include "app_zigbee.h"
#include "app_driver.h"
#include "app_config.h"
#include "app_console.h"

#include <esp_log.h>
#include <esp_console.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <stdlib.h>
#include <esp_zigbee.h>
#include <ezbee/zha.h>
${collectSlot(rendered, 'zigbee_includes')}

static const char *TAG = "app_zigbee";
${refusalShim(features)}
static app_driver_handle_t s_handle = 0;
static volatile bool s_zb_started = false;${joined ? `

/* On a Zigbee network or not, on the param bus for product code (a status
 * light): set from the stack's own join, reboot and leave signals. */
static void zc_zigbee_publish_joined(bool on)
{
    app_driver_param_val_t v = {};
    v.b = on;
    app_driver_set_param(${ZIGBEE_JOINED_PARAM}, v, s_handle);
}` : ''}

${collectSlot(rendered, 'zigbee_statics')}

/* NOT ESP_ERROR_CHECK: starting one BDB mode while another is in flight
 * (steering retry during Finding & Binding, or vice versa) returns an
 * error — that is a busy condition to retry, not a reason to abort.
 * (Found on hardware: zb-bind during steering panicked the first build.) */
static esp_err_t bdb_start_top_level_commissioning(uint8_t mode_mask)
{
    esp_err_t err = esp_zigbee_err_to_esp(ezb_bdb_start_top_level_commissioning(mode_mask));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BDB mode 0x%x not started (%s) — busy, will retry", mode_mask, esp_err_to_name(err));
    }
    return err;
}

/* A one-shot esp_timer (not a task per retry): the callback runs on the shared
 * esp_timer task — off the Zigbee task — so it takes the stack lock, mirroring
 * the SDK's own alarm_timer helper. */
struct bdb_retry_ctx {
    esp_timer_handle_t timer;
    uint8_t mode_mask;
};

static void bdb_commissioning_retry_cb(void *arg)
{
    struct bdb_retry_ctx *ctx = (struct bdb_retry_ctx *)arg;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)bdb_start_top_level_commissioning(ctx->mode_mask);
        esp_zigbee_lock_release();
    }
    esp_timer_delete(ctx->timer);
    free(ctx);
}

static void schedule_bdb_commissioning_retry(uint8_t mode_mask)
{
    struct bdb_retry_ctx *ctx = (struct bdb_retry_ctx *)calloc(1, sizeof(*ctx));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to schedule BDB mode 0x%x retry", mode_mask);
        return;
    }
    ctx->mode_mask = mode_mask;
    const esp_timer_create_args_t args = {
        .callback = bdb_commissioning_retry_cb,
        .arg = ctx,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "zb_bdb_retry",
        .skip_unhandled_events = false,
    };
    if (esp_timer_create(&args, &ctx->timer) != ESP_OK ||
        esp_timer_start_once(ctx->timer, 1000 * 1000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to schedule BDB mode 0x%x retry", mode_mask);
        if (ctx->timer) {
            esp_timer_delete(ctx->timer);
        }
        free(ctx);
    }
}

static void zigbee_driver_cb(app_driver_param_id_t, app_driver_param_val_t, app_driver_handle_t, void *);
/* The source zigbee_resync_cb hands zigbee_driver_cb. Never a registered
 * solution handle (those are 1..APP_DRIVER_MAX_SOLUTIONS) nor LOCAL (0). A
 * resync is "tell the hub the state it missed", which is right for a server
 * endpoint (an attribute) and wrong for a client one: a remote's case turns the
 * param into a COMMAND to every bound device, so replaying it after a reboot
 * would switch the lights off. Controller cases skip this source. */
#define ZB_SOURCE_RESYNC ((app_driver_handle_t)0xFF)

/* Hub/controller writes → hardware. Runs ON the Zigbee task (no lock needed). */
static void zb_attribute_handler_cases(ezb_zcl_set_attr_value_message_t *message)
{
    if (!message) return;
    if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "attribute write failed, status %d", message->info.status);
        return;
    }
    uint8_t endpoint = message->info.dst_ep;
    uint16_t cluster_id = message->info.cluster_id;
    uint16_t attr_id = message->in.attribute.id;
    (void)endpoint; (void)cluster_id; (void)attr_id;
${collectSlot(rendered, 'zigbee_attr_cb_cases', 4)}
}

static void zb_attribute_handler(ezb_zcl_set_attr_value_message_t *message)
{
    app_driver_take_refusal(NULL);
    zb_attribute_handler_cases(message);
    /* A command the param's guard refused was echoed as done by its case:
     * report the real value after it (app_driver_take_refusal). */
    app_driver_param_id_t zc_refused;
    if (app_driver_take_refusal(&zc_refused) && app_driver_param_seen(zc_refused)) {
        app_driver_param_val_t zc_now = {};
        app_driver_get_param(zc_refused, &zc_now);
        zigbee_driver_cb(zc_refused, zc_now, ZB_SOURCE_RESYNC, NULL);
    }
}

static void zb_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message)
{
    if (callback_id == EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID) {
        zb_attribute_handler((ezb_zcl_set_attr_value_message_t *)message);
        return;
    }
    /* Command-style callbacks (door lock, window covering movement, …) —
     * device-type blocks hook these via zigbee_action_cases, keyed on
     * callback_id + their own message cast. */
${collectSlot(rendered, 'zigbee_action_cases', 4)}
    ESP_LOGI(TAG, "Zigbee action 0x%x", callback_id);
}


/* Hardware/param bus → ZCL attributes. Runs OFF the Zigbee task, so every
 * Zigbee call MUST be wrapped in esp_zigbee_lock_acquire/release (compiles fine
 * without it, corrupts state at runtime). */
static void zigbee_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source; /* read by controller cases (ZB_SOURCE_RESYNC) */
    (void)ctx;
    if (!s_zb_started) return;
${collectSlot(rendered, 'zigbee_driver_cb_cases', 4)}
}

/* Push every bus param to its ZCL attribute once the device is on the network.
 * Anything written to the bus while the stack was still starting — the NVS
 * restore of a remembered brightness, a controller's boot-time decision — was
 * dropped by zigbee_driver_cb's started-gate, and the hub kept showing the
 * endpoint's creation-time defaults until the next change. Runs from the
 * esp_timer task, where the driver-cb cases' lock acquire is the normal path.
 * A param never written since boot is skipped: get_param returns the zeroed
 * union for it, a default rather than state, and pushing it would overwrite
 * the endpoint's own creation-time value with 0. */
static void zigbee_resync_cb(void *arg)
{
    (void)arg;
    for (int id = 0; id < (int)APP_DRIVER_PARAM_MAX; id++) {
        if (!app_driver_param_seen((app_driver_param_id_t)id)) continue;
        app_driver_param_val_t val = {};
        if (app_driver_get_param((app_driver_param_id_t)id, &val) != ESP_OK) continue;
        zigbee_driver_cb((app_driver_param_id_t)id, val, ZB_SOURCE_RESYNC, NULL);
    }
    ESP_LOGI(TAG, "Bus state pushed to the hub");
}

static void zigbee_schedule_resync(void)
{
    static esp_timer_handle_t s_resync = NULL;
    if (!s_resync) {
        const esp_timer_create_args_t args = {
            .callback = &zigbee_resync_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "zb_resync",
            .skip_unhandled_events = true,
        };
        if (esp_timer_create(&args, &s_resync) != ESP_OK) return;
    }
    (void)esp_timer_stop(s_resync);
    (void)esp_timer_start_once(s_resync, 200 * 1000);
}

/* REQUIRED extern: BDB/ZDO lifecycle. Behavior blocks contribute
 * zigbee_signal_cases (sig_type + err_status are in scope). */
static bool zigbee_app_signal_handler(const ezb_app_signal_t *app_signal)
{
    ezb_app_signal_type_t sig_type = ezb_app_signal_get_type(app_signal);
    const void *signal_params = ezb_app_signal_get_params(app_signal);
    ezb_bdb_comm_status_t err_status = EZB_BDB_STATUS_SUCCESS;
    if ((sig_type >> 8) == EZB_APP_SIGNAL_GROUP_BDB && signal_params) {
        err_status = ((const ezb_bdb_signal_simple_params_t *)signal_params)->status;
    }
    switch (sig_type) {
        case EZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Zigbee stack initialized");
            (void)bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
            break;
        case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case EZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == EZB_BDB_STATUS_SUCCESS) {
                if (ezb_bdb_is_factory_new()) {
                    ESP_LOGI(TAG, "Start network steering — open pairing on your Zigbee hub");${pub(false, '                    ')}
                    (void)bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
                } else {
                    ESP_LOGI(TAG, "Device rebooted on network");${pub(true, '                    ')}
                    zigbee_schedule_resync();
                }
            } else {
                ESP_LOGW(TAG, "Stack start failed (status 0x%x), retrying", err_status);
                schedule_bdb_commissioning_retry(EZB_BDB_MODE_INITIALIZATION);
            }
            break;
        case EZB_BDB_SIGNAL_STEERING:
            if (err_status == EZB_BDB_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Joined network successfully");${pub(true, '                ')}
                zigbee_schedule_resync();
            } else {
                ESP_LOGI(TAG, "Network steering not successful (status 0x%x), retrying in 1s",
                         err_status);
                schedule_bdb_commissioning_retry(EZB_BDB_MODE_NETWORK_STEERING);
            }
            break;
        case EZB_ZDO_SIGNAL_LEAVE: {
            /* A leave WITHOUT rejoin (removed from the hub, or told to leave)
             * clears the network data, and nothing else would ever look for a
             * network again — the device stayed off-network until a power
             * cycle. Steer again, as esp-zigbee's examples do. A leave WITH
             * rejoin is the stack's own business: it rejoins by itself. */
            const ezb_zdo_signal_leave_params_t *leave = (const ezb_zdo_signal_leave_params_t *)signal_params;
            if (leave && leave->leave_type == EZB_ZDO_LEAVE_TYPE_RESET) {
                ESP_LOGW(TAG, "Left the Zigbee network — steering again; open pairing on your hub");${pub(false, '                ')}
                schedule_bdb_commissioning_retry(EZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGW(TAG, "Left the Zigbee network (rejoining)");${pub(false, '                ')}
            }
            break;
        }
        default:
            ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: 0x%x",
                     ezb_app_signal_to_string(sig_type), sig_type, err_status);
            break;
    }
${collectSlot(rendered, 'zigbee_signal_cases', 4)}
    return true;
}

${epFns.join('\n\n')}

/* The Zigbee stack owns its own task — app_main must NOT block on joining. */
static void zigbee_task(void *pvParameters)
{
    (void)pvParameters;
    esp_zigbee_config_t zb_cfg = {};
    zb_cfg.device_config.device_type = EZB_NWK_DEVICE_TYPE_ROUTER;
    zb_cfg.device_config.install_code_policy = false;
    zb_cfg.device_config.zczr_config.max_children = 10;
    zb_cfg.platform_config.storage_partition_name = "nvs";
    zb_cfg.platform_config.radio_config.radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE;
    ESP_ERROR_CHECK(esp_zigbee_init(&zb_cfg));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_app_signal_add_handler(zigbee_app_signal_handler)));

    ezb_af_device_desc_t dev_desc = ezb_af_create_device_desc();
    if (dev_desc == EZB_INVALID_AF_DEVICE_DESC) {
        ESP_LOGE(TAG, "Failed to create Zigbee device descriptor");
        ESP_ERROR_CHECK(esp_zigbee_deinit());
        vTaskDelete(NULL);
        return;
    }
${epCalls.join('\n')}

    /* Node-level features contributed by behavior blocks (OTA, reporting, …) */
${collectSlot(rendered, 'zigbee_node_init', 4)}

    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_device_desc_register(dev_desc)));
    ezb_zcl_core_action_handler_register(zb_action_handler);
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_bdb_set_primary_channel_set(0x07FFF800U)));
    ESP_ERROR_CHECK(esp_zigbee_start(false));
    s_zb_started = true;
    ESP_LOGI(TAG, "Zigbee initialized");
    (void)esp_zigbee_launch_mainloop();
    s_zb_started = false;
    ezb_app_signal_remove_handler(zigbee_app_signal_handler);
    ESP_ERROR_CHECK(esp_zigbee_deinit());
    vTaskDelete(NULL);
}

/* Finding & Binding initiator: creates ZDO bindings from this node's
 * client clusters to matching targets whose F&B window is open (hubs open
 * it from their UI; devices via their own zb-bind). Controller device types
 * (remotes, scene buttons) need this once to know where their commands go. */
esp_err_t app_zigbee_start_finding_binding(void)
{
    if (!s_zb_started) {
        ESP_LOGW(TAG, "Zigbee stack not started yet");
        return ESP_ERR_INVALID_STATE;
    }
    if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = esp_zigbee_err_to_esp(
        ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_FINDING_N_BINDING));
    esp_zigbee_lock_release();
    ESP_LOGI(TAG, "Finding & Binding started (%s)", esp_err_to_name(err));
    return err;
}

static int zb_bind_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return app_zigbee_start_finding_binding() == ESP_OK ? 0 : 1;
}

esp_err_t app_zigbee_init(void)
{
    s_handle = app_driver_register_solution("zigbee", zigbee_driver_cb, NULL);

    esp_console_cmd_t bind_cmd = {};
    bind_cmd.command = "zb-bind";
    bind_cmd.help = "Start Zigbee Finding & Binding (bind this device's controls to targets)";
    bind_cmd.func = &zb_bind_cmd;
    app_console_register_cmd(&bind_cmd);

    xTaskCreate(zigbee_task, "zigbee_main", 8192, NULL, 5, NULL);
    return ESP_OK;
}
`
}

export const zigbeeGlue: FrameworkGlue = {
  id: 'zigbee',
  files(ctx) {
    const { rendered, features, hasDeviceTypes } = ctx
    const out: GeneratedFile[] = []
    out.push({ path: 'components/app_zigbee/app_zigbee.cpp', content: genAppZigbeeCpp(rendered, features) })
    if (hasDeviceTypes) {
      out.push({ path: 'components/app_zigbee/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_zigbee.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__esp-zigbee-lib app_driver app_config app_console console app_update esp_partition esp_timer
)
` })
    }
    return out
  },
}
