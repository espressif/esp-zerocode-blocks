// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/countdown_off on the host, against the REAL param bus: a countdown
// set while the load is off still runs, its end switches the load off and
// reads 0, switching the load off cancels it, and 0 cancels it.
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/countdown_off/slots')
const cfg = { max_minutes: 1440, off_cancels: true, turns_on: false, target_param: 'APP_DRIVER_PARAM_POWER', minutes_param: 'APP_DRIVER_PARAM_OFF_TIMER', remaining_param: 'APP_DRIVER_PARAM_OFF_LEFT' }
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'CD', prefix_lc: 'cd', cfg })

async function generateTypes() {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'plug')
  const product = {
    id: 'plug', name: 'plug', frameworks: ['mqtt'],
    exclude: ['behaviors/ota_basic', 'behaviors/network_status_log', 'behaviors/uptime_log', 'behaviors/wifi_disconnect_recovery'],
    instances: [
      { block: 'drivers/relay', prefix: 'APP', cfg: { gpio: 7, active_level: 1, param_id: 'APP_DRIVER_PARAM_POWER' } },
      { block: 'behaviors/countdown_off', prefix: 'CD', cfg },
    ],
  }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c6' })
  return { inc: join(outDir, 'components/app_driver/include'), cleanup: () => { rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true }) } }
}

const HARNESS = (defines, statics, init) => String.raw`
#include <cstdio>
#include <cstdint>
#include <string>
#include "app_driver.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_ERROR_CHECK(x) (void)(x)
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
static const char *TAG = "t";
typedef void (*esp_timer_cb_t)(void *);
enum esp_timer_dispatch_t { ESP_TIMER_TASK };
struct esp_timer_create_args_t { esp_timer_cb_t callback; void *arg; esp_timer_dispatch_t dispatch_method; const char *name; bool skip_unhandled_events; };
struct zc_timer { esp_timer_cb_t cb; bool running; };
typedef zc_timer *esp_timer_handle_t;
static zc_timer g_timer;
static esp_err_t esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *h) { g_timer.cb = a->callback; *h = &g_timer; return ESP_OK; }
static esp_err_t esp_timer_start_periodic(esp_timer_handle_t h, uint64_t) { h->running = true; return ESP_OK; }
static esp_err_t esp_timer_stop(esp_timer_handle_t h) { h->running = false; return ESP_OK; }
${defines}
${statics}
static void seconds(int n) { for (int i = 0; i < n; i++) { zc_test_now_us += 1000000; if (g_timer.running) g_timer.cb(nullptr); } }
static int get(int id) { app_driver_param_val_t v = {}; app_driver_get_param((app_driver_param_id_t)id, &v); return id == APP_DRIVER_PARAM_POWER ? v.b : v.u16; }
static void set_b(int id, bool b) { app_driver_param_val_t v = {}; v.b = b; app_driver_set_param((app_driver_param_id_t)id, v, APP_DRIVER_SOURCE_LOCAL); }
static void set_u(int id, int u) { app_driver_param_val_t v = {}; v.u16 = (uint16_t)u; app_driver_set_param((app_driver_param_id_t)id, v, APP_DRIVER_SOURCE_LOCAL); }
static std::string out;
static void mark(void) { out += std::to_string(get(APP_DRIVER_PARAM_POWER)) + "/" + std::to_string(get(APP_DRIVER_PARAM_OFF_TIMER)) + "/" + std::to_string(get(APP_DRIVER_PARAM_OFF_LEFT)) + " "; }
int main()
{
    zc_test_now_us = 1;
${init}
    set_u(APP_DRIVER_PARAM_OFF_TIMER, 2); mark();          /* set while off: runs */
    seconds(61); mark();                  /* 1 min left */
    set_b(APP_DRIVER_PARAM_POWER, true); mark();     /* switched on meanwhile: still runs */
    seconds(60); mark();                  /* the end: off, reads 0 */
    set_b(APP_DRIVER_PARAM_POWER, true); set_u(APP_DRIVER_PARAM_OFF_TIMER, 5); seconds(10); set_b(APP_DRIVER_PARAM_POWER, false); mark();  /* off cancels */
    set_b(APP_DRIVER_PARAM_POWER, true); set_u(APP_DRIVER_PARAM_OFF_TIMER, 3); set_u(APP_DRIVER_PARAM_OFF_TIMER, 0); seconds(400); mark();      /* 0 cancels */
    set_u(APP_DRIVER_PARAM_OFF_TIMER, 1); set_u(APP_DRIVER_PARAM_OFF_TIMER, 2); seconds(90); mark(); seconds(31); mark();             /* a new value restarts */
    printf("%s\n", out.c_str());
    return 0;
}
`

test('countdown_off: set while off, end switches off and reads 0, off and 0 cancel, a new value restarts', { skip: !haveCxx && 'no host C++ compiler' }, async () => {
  const { inc, cleanup } = await generateTypes()
  const dir = mkdtempSync(join(tmpdir(), 'zc-cd-'))
  try {
    const init = fill('logic_init.c')
    writeFileSync(join(dir, 'main.cpp'), HARNESS(fill('config_defines.h'), fill('logic_statics.c'), init))
    execFileSync(CXX, ['-std=c++20', '-Wno-unused-function', '-Wno-unused-parameter', '-pthread',
      '-I', join(BUS, 'test/shims'), '-I', inc, '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'main.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    const got = execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim()
    assert.equal(got, '0/2/2 0/2/1 1/2/1 0/0/0 0/0/0 1/0/0 1/2/1 0/0/0')
  } finally {
    rmSync(dir, { recursive: true, force: true })
    cleanup()
  }
})
