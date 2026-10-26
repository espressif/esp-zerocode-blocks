// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// frameworks/matter publishes whether the device is on any fabric
// (APP_DRIVER_PARAM_MATTER_COMMISSIONED) and whether it is advertising for
// pairing (APP_DRIVER_PARAM_MATTER_PAIRING_OPEN) on the param bus. The value
// comes from the fabric table, at boot and on every fabric event, so a reboot
// with the router down, a removal from one of several fabrics and a cancelled
// pairing all leave it right.
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

const OWNED = {
  APP_DRIVER_PARAM_MATTER_COMMISSIONED: 'bool',
  APP_DRIVER_PARAM_MATTER_PAIRING_OPEN: 'bool',
}
const matterFw = (owned = OWNED) => ({
  block: { id: 'frameworks/matter', kind: 'framework', owned_params: owned },
  prefix: '', cfg: {}, slots: {},
})

function assembled() {
  const out = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(out, 'code_blocks'), '--products-out', join(out, 'product_configurations')], { stdio: 'ignore' })
  return out
}

async function generateProduct(id, mutate = (p) => p) {
  const tpl = assembled()
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), id)
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const entry = (await eng.listAllProducts(paths)).find((p) => (p.product?.id ?? p.id) === id)
  assert.ok(entry, `${id} in the catalog`)
  const cleanup = () => {
    rmSync(dirname(tpl), { recursive: true, force: true })
    rmSync(dirname(outDir), { recursive: true, force: true })
  }
  try {
    await eng.generate(paths, { product: mutate(structuredClone(entry.product)), board: null, outDir, chip: 'esp32c6' })
  } catch (e) {
    cleanup()
    throw e
  }
  return { outDir, cleanup }
}

test('a Matter product gets both params in its enum and the writes in app_matter.cpp', async () => {
  const { outDir, cleanup } = await generateProduct('smart-plug')
  try {
    const types = readFileSync(join(outDir, 'components/app_driver/include/app_driver_types.h'), 'utf8')
    assert.match(types, /APP_DRIVER_PARAM_MATTER_COMMISSIONED,\s+\/\* bool/)
    assert.match(types, /APP_DRIVER_PARAM_MATTER_PAIRING_OPEN,\s+\/\* bool/)
    const src = readFileSync(join(outDir, 'components/app_matter/app_matter.cpp'), 'utf8')
    assert.match(src, /FabricCount\(\) > 0;\n\s+app_driver_set_param\(APP_DRIVER_PARAM_MATTER_COMMISSIONED/)
    const start = src.indexOf('esp_matter::start(app_matter_event_cb)')
    const sched = src.indexOf('ScheduleWork(zc_matter_publish_boot_state')
    assert.ok(start > 0 && sched > start, 'the boot value is read on the Matter thread after start()')
  } finally {
    cleanup()
  }
})

test('a block naming the param with another type is refused', async () => {
  await assert.rejects(generateProduct('smart-plug', (p) => ({
    ...p,
    instances: [...p.instances, { block: 'behaviors/commissioning_led', prefix: 'X', cfg: { pattern_param: 'APP_DRIVER_PARAM_MATTER_COMMISSIONED' } }],
  })), /APP_DRIVER_PARAM_MATTER_COMMISSIONED: conflicting types/)
})

test('a framework block that does not declare the params gets no writes (older catalog)', () => {
  const src = eng.genAppMatterCpp([matterFw({})])
  assert.doesNotMatch(src, /MATTER_COMMISSIONED|MATTER_PAIRING_OPEN|zc_matter_publish/)
})

test('owned_params must be APP_DRIVER_PARAM_ names with a bus type', () => {
  const bad = eng.validateBlock({ id: 'frameworks/x', kind: 'framework', owned_params: { matter_commissioned: 'bool', APP_DRIVER_PARAM_Y: 'float' } })
  const msgs = bad.errors.map((e) => e.message).join('\n')
  assert.match(msgs, /owned_params\.matter_commissioned: a param name is APP_DRIVER_PARAM_<NAME>/)
  assert.match(msgs, /owned_params\.APP_DRIVER_PARAM_Y has invalid type 'float'/)
  assert.equal(eng.validateBlock({ id: 'frameworks/x', kind: 'framework', owned_params: OWNED }).errors.length, 0)
})

/* The generated publish functions and event handler, compiled on the host
 * against a stub of the fabric table, the commissioning window and the bus. */
const HARNESS = (generated) => String.raw`
#include <stdio.h>
#include <stdint.h>
#include <string>
#define ESP_LOGW(tag, fmt, ...) ((void)(tag))
#define ESP_LOGE(tag, fmt, ...) ((void)(tag))
#define CHIP_NO_ERROR 0
typedef int esp_err_t;
typedef uint8_t app_driver_handle_t;
typedef enum { APP_DRIVER_PARAM_NONE, APP_DRIVER_PARAM_MATTER_COMMISSIONED, APP_DRIVER_PARAM_MATTER_PAIRING_OPEN } app_driver_param_id_t;
typedef union { bool b; uint8_t u8; } app_driver_param_val_t;
static std::string g_trace;
static int g_fabrics = 0;
static bool g_window = false;
static esp_err_t app_driver_set_param(app_driver_param_id_t id, app_driver_param_val_t v, app_driver_handle_t)
{
    g_trace += (id == APP_DRIVER_PARAM_MATTER_COMMISSIONED ? "C=" : "P=");
    g_trace += v.b ? "1 " : "0 ";
    return 0;
}
static void app_factory_reset_hook(void) { g_trace += "hook "; }
namespace esp_matter { static void factory_reset(void) { g_trace += "reset "; } }
namespace chip {
struct FabricTable { int FabricCount() const { return g_fabrics; } };
struct CommissioningWindowManager { bool IsCommissioningWindowOpen() const { return g_window; } };
struct Server {
    static Server &GetInstance() { static Server s; return s; }
    FabricTable &GetFabricTable() { static FabricTable t; return t; }
    CommissioningWindowManager &GetCommissioningWindowManager() { static CommissioningWindowManager m; return m; }
};
namespace DeviceLayer {
struct DeviceEventType { enum { kCommissioningComplete = 1, kFabricCommitted, kFabricRemoved, kCommissioningWindowOpened, kCommissioningWindowClosed, kInternetConnectivityChange }; };
struct ChipDeviceEvent { uint16_t Type; };
}
}
static const char *TAG = "t";
static app_driver_handle_t s_handle = 1;
${generated}
static void ev(uint16_t type) { chip::DeviceLayer::ChipDeviceEvent e{type}; app_matter_event_cb(&e, 0); }
int main(void)
{
    using T = chip::DeviceLayer::DeviceEventType;
    g_fabrics = 2; g_window = false;
    zc_matter_publish_boot_state(0);              /* rebooted on two fabrics, router down */
    ev(T::kInternetConnectivityChange);           /* connectivity says nothing */
    ev(T::kFabricRemoved); g_trace += "| ";       /* still 2: nothing changes yet */
    g_fabrics = 1; ev(T::kFabricRemoved);         /* removed from one of two */
    g_trace += "| ";
    g_fabrics = 0; ev(T::kFabricRemoved);         /* the last one */
    g_trace += "| ";
    ev(T::kCommissioningWindowOpened);
    ev(T::kCommissioningWindowClosed);            /* a cancelled pairing: no fabric */
    g_trace += "| ";
    g_fabrics = 1; ev(T::kCommissioningComplete);
    printf("%s\n", g_trace.c_str());
    return 0;
}
`

function cut(src, startMarker, endMarker) {
  const a = src.indexOf(startMarker)
  const b = src.indexOf(endMarker, a)
  assert.ok(a >= 0 && b > a, `${startMarker} .. ${endMarker}`)
  return src.slice(a, b)
}

test('the published state follows the fabric table through reboot, partial removal and a cancelled pairing', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const src = eng.genAppMatterCpp([matterFw()])
  const cb = src.indexOf('static void app_matter_event_cb')
  const generated = cut(src, '/* True while the device is on at least one fabric', 'static void app_matter_event_cb') +
    src.slice(cb, src.indexOf('\n}\n', cb) + 3)
  const dir = mkdtempSync(join(tmpdir(), 'zc-matter-state-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), HARNESS(generated))
    execFileSync(CXX, ['-std=c++17', '-Wall', '-Werror', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')])
    const out = execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim()
    // Our fake bus records every write; the real one dedupes repeats.
    assert.equal(out, 'C=1 P=0 C=1 | C=1 | C=0 hook reset | P=1 P=0 | C=1')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
