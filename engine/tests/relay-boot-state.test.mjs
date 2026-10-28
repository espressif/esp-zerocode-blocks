// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// An on/off output driver (drivers/relay, relay_with_button, board_gpio_ctrl)
// writes the state it drove the output to at boot — off — onto its param
// during app_driver_init, so the param counts as written from the start
// (app_driver_param_seen) instead of staying "never written" until product
// code first switches it. A value a block restored earlier is left alone, and
// behaviors/nvs_persist_bool still restores a saved ON over it. The generated
// app_driver.cpp and persist code run here on the host against the REAL param
// bus (base_firmware's app_driver_cb.cpp) over a stubbed GPIO and NVS.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))
const CXX = process.env.CXX || 'c++'
const haveCxx = spawnSync(CXX, ['--version']).status === 0
const SHIMS = join(ROOT, 'base_firmware/components/app_driver/test/shims')
const EXCLUDE = ['behaviors/ota_basic', 'behaviors/network_status_log', 'behaviors/uptime_log', 'behaviors/wifi_disconnect_recovery']

async function generate(t, driver) {
  const root = mkdtempSync(join(tmpdir(), 'zc-boot-state-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const tpl = join(root, 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(root, 'out')
  const product = {
    id: 'plug', name: 'plug', frameworks: [], exclude: EXCLUDE,
    instances: [driver, { block: 'behaviors/nvs_persist_bool', prefix: 'PERSIST', cfg: { target_param: 'APP_DRIVER_PARAM_POWER', nvs_key: 'power' } }],
  }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c6' })
  return { root, outDir }
}

function between(src, start, end) {
  const a = src.indexOf(start)
  const b = src.indexOf(end, a + start.length)
  assert.ok(a >= 0 && b > a, `${start} .. ${end}`)
  return src.slice(a, b)
}

const GPIO_H = String.raw`
#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef int gpio_num_t;
enum { GPIO_MODE_OUTPUT = 2, GPIO_PULLUP_DISABLE = 0, GPIO_PULLDOWN_DISABLE = 0, GPIO_INTR_DISABLE = 0 };
typedef struct { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; } gpio_config_t;
inline int g_level[64];
static inline esp_err_t gpio_set_level(gpio_num_t n, uint32_t l) { g_level[n] = (int)l; return ESP_OK; }
static inline esp_err_t gpio_config(const gpio_config_t *) { return ESP_OK; }
#define ESP_ERROR_CHECK(x) ((void)(x))
`
const NVS_H = String.raw`
#pragma once
#include <stdint.h>
#include "esp_err.h"
#define ESP_ERR_NVS_NOT_FOUND 0x1102
typedef int nvs_handle_t;
enum { NVS_READWRITE = 1 };
inline int g_nvs_power = -1;      /* -1: never saved */
inline int g_nvs_writes = 0;
static inline esp_err_t nvs_open(const char *, int, nvs_handle_t *h) { *h = 1; return ESP_OK; }
static inline esp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *v) { if (g_nvs_power < 0) return ESP_ERR_NVS_NOT_FOUND; *v = (uint8_t)g_nvs_power; return ESP_OK; }
static inline esp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t v) { g_nvs_power = v; g_nvs_writes++; return ESP_OK; }
static inline esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
static inline void nvs_close(nvs_handle_t) {}
`

function harness(persist) {
  return String.raw`
#include "app_driver.h"
#include "app_config.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <freertos/portmacro.h>
#include <cstdio>
#include <cstring>
#include <string>
static const char *TAG = "persist";
${persist}
static void report(void)
{
    app_driver_param_val_t v = {};
    app_driver_get_param(APP_DRIVER_PARAM_POWER, &v);
    printf("seen=%d on=%d pin=%d nvs=%d writes=%d ", app_driver_param_seen(APP_DRIVER_PARAM_POWER) ? 1 : 0, v.b ? 1 : 0, g_level[7], g_nvs_power, g_nvs_writes);
}
int main(int argc, char **argv)
{
    std::string s = argc > 1 ? argv[1] : "";
    if (s == "saved-on") g_nvs_power = 1;
    if (s == "saved-off") g_nvs_power = 0;
    if (s == "written-first") {          /* another block wrote it before this driver's init */
        app_driver_param_val_t v = {}; v.b = true;
        app_driver_set_param(APP_DRIVER_PARAM_POWER, v, APP_DRIVER_SOURCE_RESTORE);
    }
    app_driver_init();                  /* boot order, as app_main runs it */
    report();
    zc_persist_init();                  /* zc_behaviors_init: the persist restore */
    report();
    printf("\n");
    return 0;
}
`
}

function build(t, outDir, root) {
  const stubs = join(root, 'stubs')
  mkdirSync(join(stubs, 'driver'), { recursive: true })
  writeFileSync(join(stubs, 'driver/gpio.h'), GPIO_H)
  writeFileSync(join(stubs, 'nvs_flash.h'), NVS_H)
  const persistSrc = readFileSync(join(outDir, 'components/app_logic/persist.cpp'), 'utf-8')
  const persist = between(persistSrc, '/* behaviors/nvs_persist_bool (PERSIST) */', 'void persist_logic_init')
  writeFileSync(join(root, 'main.cpp'), harness(persist))
  const bin = join(root, 'p')
  execFileSync(CXX, ['-std=c++17', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-missing-field-initializers', '-pthread', '-DAPP_DRIVER_POLL_NO_TASK',
    '-I', stubs, '-I', SHIMS, '-I', join(outDir, 'components/app_driver/include'), '-I', join(outDir, 'components/app_config/include'),
    join(root, 'main.cpp'), join(outDir, 'components/app_driver/app_driver.cpp'), join(outDir, 'components/app_driver/app_driver_cb.cpp'),
    '-o', bin], { stdio: 'pipe' })
  return (scenario) => {
    const out = spawnSync(bin, [scenario], { encoding: 'utf-8' })
    assert.equal(out.status, 0, out.stdout + out.stderr)
    return out.stdout.trim()
  }
}

const DRIVERS = {
  'drivers/relay': { block: 'drivers/relay', prefix: 'PLUG', cfg: { gpio: 7, active_level: 0, param_id: 'APP_DRIVER_PARAM_POWER' } },
  'drivers/relay_with_button': { block: 'drivers/relay_with_button', prefix: 'PLUG', cfg: { relay_gpio: 7, relay_active_level: 0, button_gpio: 9, param_id: 'APP_DRIVER_PARAM_POWER' } },
}

for (const [id, driver] of Object.entries(DRIVERS)) {
  test(`${id}: the output's boot state is written to its param in app_driver_init, and a saved ON still restores`, { skip: !haveCxx && 'no host C++ compiler' }, async (t) => {
    const { root, outDir } = await generate(t, driver)
    const run = build(t, outDir, root)
    // Active-LOW relay: pin 1 is off, 0 is on.
    // Fresh device: off and written from app_driver_init on; the persist
    // listener registered after it, so the boot write saves nothing.
    assert.equal(run('fresh'), 'seen=1 on=0 pin=1 nvs=-1 writes=0 seen=1 on=0 pin=1 nvs=-1 writes=0')
    // A saved ON is restored over the boot state and drives the relay, and NVS
    // keeps it (the restore's own change re-saves the same 1, as it always did).
    // writes=0: the persist block writes on the bus's poll task, never inside the restore.
    assert.equal(run('saved-on'), 'seen=1 on=0 pin=1 nvs=1 writes=0 seen=1 on=1 pin=0 nvs=1 writes=0')
    assert.equal(run('saved-off'), 'seen=1 on=0 pin=1 nvs=0 writes=0 seen=1 on=0 pin=1 nvs=0 writes=0')
    // A value written before the driver's boot state is not overwritten with off.
    assert.match(run('written-first'), /^seen=1 on=1 pin=\d nvs=-1 writes=0 seen=1 on=1 pin=\d nvs=-1 writes=0$/)
  })
}

test('every on/off output driver whose output is off at boot says so on its param, guarded on never-written', () => {
  for (const id of ['relay', 'relay_with_button', 'board_gpio_ctrl']) {
    const src = readFileSync(join(ROOT, 'code_blocks/drivers', id, 'slots/driver_boot_state.c'), 'utf-8')
    assert.match(src, /if \(!app_driver_param_seen\(\{\{cfg\.param_id\}\}\)\)/, id)
    assert.match(src, /app_driver_set_param\(\{\{cfg\.param_id\}\}, [a-z_{}]+, APP_DRIVER_SOURCE_RESTORE\)/, id)
  }
})
