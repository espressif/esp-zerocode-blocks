// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { RenderedInstance } from '../types.js'
import { type CatalogFeatures, isFramework, collectSlot, CURRENT_CATALOG, refusalShim, indentText, unwrapInitBody } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

/** The params frameworks/matter publishes about itself (its `owned_params`),
 *  emitted only for the ones the framework block declares — the declaration is
 *  what puts them in the param enum. Every write runs on the Matter thread: a
 *  scheduled work item right after esp_matter::start() for the boot value, the
 *  event handler afterwards, so the fabric table and the commissioning window
 *  are read under the stack lock and in event order. */
const MATTER_COMMISSIONED_PARAM = 'APP_DRIVER_PARAM_MATTER_COMMISSIONED'
const MATTER_PAIRING_OPEN_PARAM = 'APP_DRIVER_PARAM_MATTER_PAIRING_OPEN'

function matterStateCode(rendered: RenderedInstance[]): { statics: string; eventCases: string; afterStart: string } {
  const owned = new Set(rendered.filter(r => isFramework(r, 'matter')).flatMap(r => Object.keys(r.block.owned_params ?? {})))
  const commissioned = owned.has(MATTER_COMMISSIONED_PARAM)
  const pairing = owned.has(MATTER_PAIRING_OPEN_PARAM)
  if (!commissioned && !pairing) return { statics: '', eventCases: '', afterStart: '' }
  const fns: string[] = []
  const boot: string[] = []
  const cases: string[] = []
  if (commissioned) {
    fns.push(`/* True while the device is on at least one fabric. Read from the fabric
 * table, never inferred from an event: a fabric removed while others remain
 * leaves it true, a cancelled pairing never set it. */
static void zc_matter_publish_commissioned(void)
{
    app_driver_param_val_t v = {};
    v.b = chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
    app_driver_set_param(${MATTER_COMMISSIONED_PARAM}, v, s_handle);
}`)
    boot.push('    zc_matter_publish_commissioned();')
    cases.push(`    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        zc_matter_publish_commissioned();
        break;`)
  }
  if (pairing) {
    fns.push(`/* True while the device advertises itself for pairing (BLE / DNS-SD): from
 * the window opening until a phone connects to pair, the window times out or
 * is closed. A failed attempt that re-advertises sets it again. */
static void zc_matter_publish_pairing_open(bool open)
{
    app_driver_param_val_t v = {};
    v.b = open;
    app_driver_set_param(${MATTER_PAIRING_OPEN_PARAM}, v, s_handle);
}`)
    boot.push('    zc_matter_publish_pairing_open(chip::Server::GetInstance().GetCommissioningWindowManager().IsCommissioningWindowOpen());')
    cases.push(`    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        zc_matter_publish_pairing_open(true);
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        zc_matter_publish_pairing_open(false);
        break;`)
  }
  const statics = `${fns.join('\n\n')}

static void zc_matter_publish_boot_state(intptr_t)
{
${boot.join('\n')}
}

`
  const eventCases = `    switch (event->Type) {
${cases.join('\n')}
    default:
        break;
    }
`
  const afterStart = `    /* The boot value, on the Matter thread after everything start() queued. */
    if (chip::DeviceLayer::PlatformMgr().ScheduleWork(zc_matter_publish_boot_state, 0) != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Failed to schedule the Matter state read");
    }
`
  return { statics, eventCases, afterStart }
}

export function genAppMatterCpp(rendered: RenderedInstance[], features: CatalogFeatures = CURRENT_CATALOG): string {
  // Each endpoint's create block becomes its own static function — isolating
  // its `cfg`/`ep` locals (which would collide across endpoints in one scope)
  // and removing the bare `{ }` block. The body's `return ESP_FAIL` now returns
  // from this function, which the caller checks.
  const usedEp = new Set<string>()
  const epFns: string[] = []
  const epCalls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['matter_endpoint_create'] ?? '').trim()) continue
    const slug = (r.prefix || r.block.id.split('/').pop() || 'ep').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; usedEp.has(name); n++) name = `${slug}_${n}`
    usedEp.add(name)
    epFns.push(`static esp_err_t zc_${name}_matter_endpoint_init(node_t *node)\n{\n${indentText(unwrapInitBody(r.slots['matter_endpoint_create']), 4)}\n    return ESP_OK;\n}`)
    epCalls.push(`    if (zc_${name}_matter_endpoint_init(node) != ESP_OK) return ESP_FAIL;`)
  }
  // Defaulted slot: a `matter_last_fabric_cases` contribution replaces this code.
  // Some handler must exist — with none, a device removed from its last fabric
  // can never be commissioned again.
  // The same reset as every other path (console, button, power cycle):
  // app_utils_factory_reset runs the product hook, erases the WHOLE NVS
  // partition and reboots. esp_matter::factory_reset() clears only Matter's
  // and CHIP's namespaces, so a product's saved state (a relay restored at
  // power-up) came back after "out-of-box".
  const lastFabricCases = collectSlot(rendered, 'matter_last_fabric_cases', 4) ||
`    if (event->Type == chip::DeviceLayer::DeviceEventType::kFabricRemoved &&
        chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
        ESP_LOGW(TAG, "Last fabric removed — factory reset to out-of-box state");
        ${features.utilsFactoryReset ? 'app_utils_factory_reset();' : 'esp_matter::factory_reset();'}
    }`
  const state = matterStateCode(rendered)
  return `#include "app_matter.h"
#include "app_driver.h"
#include "app_config.h"
${features.utilsFactoryReset ? '\n#include "app_utils.h"\n' : ''}
#include <esp_log.h>
#include <esp_matter.h>
#include <app/server/Server.h>
#if CONFIG_ENABLE_MATTER_OVER_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif
${collectSlot(rendered, 'matter_includes')}

using namespace esp_matter;
using namespace chip::app::Clusters;

static const char *TAG = "app_matter";
${refusalShim(features)}
static app_driver_handle_t s_handle = 0;
/* "This attribute update is the driver's own echo — do not apply it back."
 * THREAD-LOCAL, because the echo and a controller write run on different
 * tasks: matter_driver_cb sets it on the task that called set_param, then
 * attribute::update blocks there for the CHIP lock. A plain global was true
 * for that whole wait, so a controller write the CHIP task handled meanwhile
 * hit PRE_UPDATE with the flag set and was dropped — the attribute changed,
 * the hardware did not. Per task, only the update the setter itself makes
 * (PRE_UPDATE runs on the caller of attribute::update) is skipped. ESP-IDF's
 * FreeRTOS gives every task its own copy (api-guides/thread-local-storage). */
static thread_local bool s_from_driver = false;

${collectSlot(rendered, 'matter_statics')}

static void matter_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
${collectSlot(rendered, 'matter_driver_cb_cases', 4)}
}

static esp_err_t app_attribute_update_cb(
    attribute::callback_type_t type,
    uint16_t endpoint_id,
    uint32_t cluster_id,
    uint32_t attribute_id,
    esp_matter_attr_val_t *val,
    void *priv_data)
{
    if (type == attribute::PRE_UPDATE) {
        if (s_from_driver) return ESP_OK;
        app_driver_take_refusal(NULL);
${collectSlot(rendered, 'matter_attr_cb_cases', 8)}
        /* The param's guard refused it: reject the write, so the attribute
         * keeps the device's real value and the controller is told it failed. */
        if (app_driver_take_refusal(NULL)) return ESP_ERR_NOT_ALLOWED;
    }
    return ESP_OK;
}

static esp_err_t app_identification_cb(
    identification::callback_type_t type,
    uint16_t endpoint_id,
    uint8_t effect_id,
    uint8_t effect_variant,
    void *priv_data)
{
    ESP_LOGI(TAG, "Identification: type=%d, endpoint=%d, effect=%d", type, endpoint_id, effect_id);
${collectSlot(rendered, 'matter_identify_cases', 4)}
    return ESP_OK;
}

${state.statics}static void app_matter_event_cb(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t arg)
{
${state.eventCases}${lastFabricCases}
${collectSlot(rendered, 'matter_event_cases', 4)}
}

${epFns.join('\n\n')}

#if CONFIG_ENABLE_MATTER_OVER_THREAD
/* The Thread platform config, as a weak default rather than a fixed one: this
 * covers a chip driving its own 802.15.4 radio, which is every Thread-capable
 * target here (esp32c5 / esp32c6 / esp32h2). A product that needs something
 * else — an external radio co-processor over UART/SPI, a different storage
 * partition, deeper queues — defines a strong
 * zc_openthread_platform_config() in any of its components and that one
 * wins at link time, with no change to this generator or to the
 * matter_thread fragment. The RCP transports need port/baud/pin values that
 * only the board knows, which is why they are not derived from Kconfig here. */
extern "C" __attribute__((weak))
esp_openthread_platform_config_t zc_openthread_platform_config(void)
{
    esp_openthread_platform_config_t cfg = {};
    cfg.radio_config.radio_mode = RADIO_MODE_NATIVE;
    cfg.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;
    cfg.port_config.storage_partition_name = "nvs";
    cfg.port_config.netif_queue_size = 10;
    cfg.port_config.task_queue_size = 10;
    return cfg;
}
#endif

esp_err_t app_matter_init(void)
{
    s_handle = app_driver_register_solution("matter", matter_driver_cb, NULL);
${collectSlot(rendered, 'matter_init', 4)}
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return ESP_FAIL;
    }

${epCalls.join('\n')}

#if CONFIG_ENABLE_MATTER_OVER_THREAD
    /* Matter's ThreadStackManagerImpl::_InitThreadStack() passes the launcher's
     * file-static config straight to esp_openthread_init(). Nothing populates
     * that pointer unless it is set here, and the assert() guarding it is
     * compiled out at this optimization level — so leaving it unset does not
     * fail loudly, it faults on the first dereference inside the OpenThread
     * stack. */
    static esp_openthread_platform_config_t ot_platform_config =
        zc_openthread_platform_config();
    ESP_ERROR_CHECK(set_openthread_platform_config(&ot_platform_config));
#endif

    ESP_ERROR_CHECK(esp_matter::start(app_matter_event_cb));
${state.afterStart}    ESP_LOGI(TAG, "Matter initialized");
    return ESP_OK;
}
`
}

export const matterGlue: FrameworkGlue = {
  id: 'matter',
  files(ctx) {
    const { rendered, features } = ctx
    const out: GeneratedFile[] = []
    out.push({ path: 'components/app_matter/app_matter.cpp', content: genAppMatterCpp(rendered, features) })
    // Written with or without device types: the generated app_matter.cpp
    // needs app_utils, which an older catalog's own CMakeLists lacks.
    out.push({ path: 'components/app_matter/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_matter.cpp"
    INCLUDE_DIRS "include"
    REQUIRES espressif__esp_matter
    PRIV_REQUIRES app_driver app_config app_utils
)
` })
    return out
  },
}
