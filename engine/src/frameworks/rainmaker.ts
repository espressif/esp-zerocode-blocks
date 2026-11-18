// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { RenderedInstance } from '../types.js'
import { type CatalogFeatures, collectSlot, CURRENT_CATALOG, refusalShim, indentText, unwrapInitBody } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

export function genAppRainmakerCpp(rendered: RenderedInstance[], features: CatalogFeatures = CURRENT_CATALOG): string {
  // Mirror of genAppMatterCpp: each device-type instance's
  // rainmaker_device_create slot becomes its own static init function
  // (isolating locals), called in sequence from app_rainmaker_init().
  const usedDev = new Set<string>()
  const devFns: string[] = []
  const devCalls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['rainmaker_device_create'] ?? '').trim()) continue
    const slug = (r.prefix || r.block.id.split('/').pop() || 'dev').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; usedDev.has(name); n++) name = `${slug}_${n}`
    usedDev.add(name)
    devFns.push(`static esp_err_t zc_${name}_rainmaker_device_init(esp_rmaker_node_t *node)\n{\n${indentText(unwrapInitBody(r.slots['rainmaker_device_create']), 4)}\n    return ESP_OK;\n}`)
    devCalls.push(`    if (zc_${name}_rainmaker_device_init(node) != ESP_OK) return ESP_FAIL;`)
  }
  return `#include "app_rainmaker.h"
#include "app_driver.h"
#include "app_config.h"

#include <esp_log.h>
#include <esp_event.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_common_events.h>
#include <app_network.h>
#include <network_provisioning/manager.h>
${collectSlot(rendered, 'rainmaker_includes')}

static const char *TAG = "app_rainmaker";
${refusalShim(features)}
static app_driver_handle_t s_handle = 0;

${collectSlot(rendered, 'rainmaker_statics')}

static void rainmaker_driver_cb(app_driver_param_id_t, app_driver_param_val_t, app_driver_handle_t, void *);

/* Cloud/app write → hardware. Each device-type binding claims its own
 * (device, param) pair and returns; unclaimed writes fall through to OK. */
static esp_err_t rainmaker_write_cases(
    const esp_rmaker_device_t *device,
    const esp_rmaker_param_t *param,
    const esp_rmaker_param_val_t val,
    void *priv_data,
    esp_rmaker_write_ctx_t *ctx)
{
    (void)priv_data;
    if (ctx) {
        ESP_LOGI(TAG, "Write via %s: %s", esp_rmaker_device_cb_src_to_str(ctx->src),
                 esp_rmaker_param_get_name(param));
    }
${collectSlot(rendered, 'rainmaker_write_cb_cases', 4)}
    return ESP_OK;
}

static esp_err_t rainmaker_write_cb(
    const esp_rmaker_device_t *device,
    const esp_rmaker_param_t *param,
    const esp_rmaker_param_val_t val,
    void *priv_data,
    esp_rmaker_write_ctx_t *ctx)
{
    app_driver_take_refusal(NULL);
    esp_err_t err = rainmaker_write_cases(device, param, val, priv_data, ctx);
    /* A command the param's guard refused was echoed as done by its case:
     * report the real value after it (app_driver_take_refusal). */
    app_driver_param_id_t zc_refused;
    if (app_driver_take_refusal(&zc_refused) && app_driver_param_seen(zc_refused)) {
        app_driver_param_val_t zc_now = {};
        app_driver_get_param(zc_refused, &zc_now);
        rainmaker_driver_cb(zc_refused, zc_now, APP_DRIVER_SOURCE_LOCAL, NULL);
    }
    return err;
}

/* Hardware/param bus → cloud. The registry doesn't notify the source
 * solution, so writes we made ourselves in rainmaker_write_cb don't echo. */
static void rainmaker_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
${collectSlot(rendered, 'rainmaker_driver_cb_cases', 4)}
}

/* Provisioning / RainMaker / network lifecycle events (default event loop).
 * Behavior blocks contribute rainmaker_event_cases keyed on
 * (event_base, event_id) — e.g. NETWORK_PROV_EVENT / RMAKER_COMMON_EVENT. */
static void rainmaker_app_event_cb(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
${collectSlot(rendered, 'rainmaker_event_cases', 4)}
}

${devFns.join('\n\n')}

esp_err_t app_rainmaker_init(void)
{
    s_handle = app_driver_register_solution("rainmaker", rainmaker_driver_cb, NULL);

    /* Network stack first: RainMaker core needs Wi-Fi initialized before
     * esp_rmaker_node_init (self-claiming reads the MAC). */
    app_network_init();

    /* RainMaker's own SNTP client: wall-clock time for anything the product
     * keys on it (schedules, daily budgets, night windows, timestamps).
     * Without it time() sits at the 1970 epoch and every such feature
     * silently misbehaves. */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = true,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(
        &rainmaker_cfg, "ZeroCode Device", "Device");
    if (!node) {
        ESP_LOGE(TAG, "Failed to init RainMaker node");
        return ESP_FAIL;
    }

${devCalls.join('\n')}

    /* Node-level features contributed by behavior blocks (OTA, services, …) */
${collectSlot(rendered, 'rainmaker_node_init', 4)}

    esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, rainmaker_app_event_cb, NULL);
    esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID, rainmaker_app_event_cb, NULL);
    esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, rainmaker_app_event_cb, NULL);

    ESP_ERROR_CHECK(esp_rmaker_start());

    /* Starts BLE/SoftAP provisioning via the ESP RainMaker phone app when
     * unprovisioned; connects directly on subsequent boots. */
    esp_err_t err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start network: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "RainMaker initialized");
    return ESP_OK;
}
`
}

export const rainmakerGlue: FrameworkGlue = {
  id: 'rainmaker',
  files(ctx) {
    const { rendered, features, hasDeviceTypes } = ctx
    const out: GeneratedFile[] = []
    out.push({ path: 'components/app_rainmaker/app_rainmaker.cpp', content: genAppRainmakerCpp(rendered, features) })
    if (hasDeviceTypes) {
      out.push({ path: 'components/app_rainmaker/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_rainmaker.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__esp_rainmaker espressif__rmaker_app_network espressif__network_provisioning esp_event app_driver app_config
)
` })
    }
    return out
  },
}
