// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/commissioning_led on a Matter product takes "paired or not" from
// the framework's APP_DRIVER_PARAM_MATTER_COMMISSIONED (the fabric table at
// boot, re-read on every fabric event) instead of assuming "waiting to be set
// up" at every boot and clearing it only on a connectivity event. The
// generated Matter code runs here on the host against the REAL param bus
// (base_firmware's app_driver_cb.cpp) and a stub of the CHIP types it names.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))
const CXX = process.env.CXX || 'c++'
const haveCxx = spawnSync(CXX, ['--version']).status === 0
const BUS = join(ROOT, 'base_firmware/components/app_driver')
const PATTERN = 'APP_DRIVER_PARAM_LED_PATTERN'

async function generateMatterLed() {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'led')
  const product = {
    id: 'led', name: 'led', frameworks: ['matter'],
    exclude: ['behaviors/ota_basic', 'behaviors/network_status_log', 'behaviors/uptime_log', 'behaviors/wifi_disconnect_recovery'],
    instances: [{ block: 'behaviors/commissioning_led', prefix: 'COMM_LED', cfg: { pattern_param: PATTERN } }],
  }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c6' })
  return { outDir, cleanup: () => { rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true }) } }
}

function between(src, start, end) {
  const a = src.indexOf(start)
  const b = src.indexOf(end, a + start.length)
  assert.ok(a >= 0 && b > a, `${start} .. ${end}`)
  return src.slice(a + start.length, b)
}

const HARNESS = (statics, eventCb, initSlots, logicInit) => String.raw`
#include "app_driver.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#define ESP_LOGW(tag, fmt, ...) ((void)(tag))
#define ESP_LOGE(tag, fmt, ...) ((void)(tag))
#define ESP_LOGI(tag, fmt, ...) ((void)(tag))
#define CHIP_NO_ERROR 0
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
static int g_fabrics = 0;
static std::string g_trace;
static void app_factory_reset_hook(void) {}
namespace esp_matter { static void factory_reset(void) { g_trace += "reset "; } }
namespace chip {
struct FabricTable { int FabricCount() const { return g_fabrics; } };
struct CommissioningWindowManager { bool IsCommissioningWindowOpen() const { return false; } };
struct Server {
    static Server &GetInstance() { static Server s; return s; }
    FabricTable &GetFabricTable() { static FabricTable t; return t; }
    CommissioningWindowManager &GetCommissioningWindowManager() { static CommissioningWindowManager m; return m; }
};
namespace DeviceLayer {
enum ConnectivityChange { kConnectivity_NoChange, kConnectivity_Established, kConnectivity_Lost };
struct DeviceEventType { enum { kCommissioningComplete = 1, kFabricCommitted, kFabricRemoved, kCommissioningWindowOpened,
    kCommissioningWindowClosed, kInternetConnectivityChange, kCommissioningSessionStarted }; };
struct ChipDeviceEvent { uint16_t Type; struct { ConnectivityChange IPv4, IPv6; } InternetConnectivityChange; };
}
}
static const char *TAG = "t";
static app_driver_handle_t s_handle = 0;
${statics}
static void app_matter_event_cb${eventCb}
static void ev(uint16_t type, chip::DeviceLayer::ConnectivityChange v4 = chip::DeviceLayer::kConnectivity_NoChange)
{
    chip::DeviceLayer::ChipDeviceEvent e{};
    e.Type = type;
    e.InternetConnectivityChange.IPv4 = v4;
    app_matter_event_cb(&e, 0);
}
/* What app_matter_init runs (it registers the framework, then the slots). */
static void matter_init(void)
{
    s_handle = app_driver_register_solution("matter", NULL, NULL);
${initSlots}
}
static void logic_init(void)
${logicInit}
static int led(void) { app_driver_param_val_t v; app_driver_get_param(${PATTERN}, &v); return v.u8; }
static void boot_publish(void) { zc_matter_publish_boot_state(0); }
int main(int argc, char **argv)
{
    using T = chip::DeviceLayer::DeviceEventType;
    using namespace chip::DeviceLayer;
    std::string s = argc > 1 ? argv[1] : "";
    std::string out;
    auto mark = [&]() { out += std::to_string(led()) + " "; };
    if (s == "paired-boot") {             /* boot order: framework, its boot read, logic */
        g_fabrics = 2; matter_init(); boot_publish(); mark(); logic_init(); mark();
    } else if (s == "paired-boot-late-read") { /* the fabric read lands after logic_init's default */
        g_fabrics = 1; matter_init(); logic_init(); mark(); boot_publish(); mark();
    } else if (s == "paired-default-races") {  /* logic_init's default written after the read */
        g_fabrics = 1; matter_init(); boot_publish(); mark();
        app_driver_param_val_t v = {}; v.u8 = 2; app_driver_set_param(${PATTERN}, v, APP_DRIVER_SOURCE_LOCAL); mark();
    } else if (s == "unpaired-pairing") {
        g_fabrics = 0; matter_init(); boot_publish(); logic_init(); mark();
        ev(T::kInternetConnectivityChange, kConnectivity_Established); mark(); /* Wi-Fi up mid-pairing: not paired yet */
        ev(T::kCommissioningSessionStarted); mark();
        g_fabrics = 1; ev(T::kCommissioningComplete); mark();
    } else if (s == "fabrics-and-network") {
        g_fabrics = 2; matter_init(); boot_publish(); logic_init(); mark();
        g_fabrics = 1; ev(T::kFabricRemoved); mark();       /* one of two */
        ev(T::kInternetConnectivityChange, kConnectivity_Lost); mark();
        ev(T::kInternetConnectivityChange, kConnectivity_Established); mark();
        g_fabrics = 0; ev(T::kFabricRemoved); mark();       /* the last one */
        ev(T::kInternetConnectivityChange, kConnectivity_Lost); mark(); /* unpaired: stays waiting */
    }
    printf("%s\n", out.c_str());
    return 0;
}
`

test('commissioning_led derives paired / unpaired from MATTER_COMMISSIONED', { skip: !haveCxx && 'no host C++ compiler' }, async () => {
  const { outDir, cleanup } = await generateMatterLed()
  const dir = mkdtempSync(join(tmpdir(), 'zc-commled-'))
  try {
    const src = readFileSync(join(outDir, 'components/app_matter/app_matter.cpp'), 'utf8')
    const statics = between(src, 'static thread_local bool s_from_driver = false;', 'static void matter_driver_cb(') +
      between(src, 'return ESP_OK;\n}\n\n/* True while the device is on', 'static void app_matter_event_cb')
        .replace(/^/, '/* True while the device is on')
    const evStart = src.indexOf('static void app_matter_event_cb') + 'static void app_matter_event_cb'.length
    const eventCb = src.slice(evStart, src.indexOf('\n}\n', evStart) + 3)
    const initSlots = between(src, 's_handle = app_driver_register_solution("matter", matter_driver_cb, NULL);', 'node::config_t node_config;')
    const logicInit = readFileSync(join(ROOT, 'code_blocks/behaviors/commissioning_led/slots/logic_init.c'), 'utf8')
      .replaceAll('{{cfg.pattern_param}}', PATTERN)
    writeFileSync(join(dir, 'main.cpp'), HARNESS(statics, eventCb, initSlots, logicInit))
    execFileSync(CXX, ['-std=c++20', '-Wno-unused-function', '-Wno-unused-parameter', '-pthread',
      '-I', join(BUS, 'test/shims'), '-I', join(outDir, 'components/app_driver/include'), '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'main.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    const run = (s) => execFileSync(join(dir, 'a.out'), [s], { encoding: 'utf8' }).trim()
    // 1 solid (online / paired), 2 slow blink (waiting to be set up), 3 fast blink (pairing), 4 breathe (lost network)
    assert.equal(run('paired-boot'), '1 1', 'a paired device boots solid, whatever the router does')
    assert.equal(run('paired-boot-late-read'), '2 1', 'the fabric read corrects the boot default')
    assert.equal(run('paired-default-races'), '1 1', 'a waiting-to-be-set-up written on a paired device is put back')
    assert.equal(run('unpaired-pairing'), '2 2 3 1', 'unpaired waits, pairs, goes solid; Wi-Fi alone is not paired')
    assert.equal(run('fabrics-and-network'), '1 1 4 1 2 2', 'one of two fabrics removed stays paired; lost network breathes; the last fabric waits')
    assert.doesNotMatch(src, /kFabricRemoved: \{\s*app_driver_param_val_t v = \{ \.u8 = 2 \}/, 'no "unpaired" from a single fabric removal')
  } finally {
    rmSync(dir, { recursive: true, force: true })
    cleanup()
  }
})
