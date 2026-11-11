// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/button: the press that wakes the device from deep sleep is over
// before the button exists, so it was lost — a doorbell, a clicker, a battery
// sensor's button did nothing on the press that woke them. When this pin woke
// the chip and the button is already released, that press is delivered once,
// from logic_start: after app_logic_init(), so a value restored at boot
// (nvs_persist) cannot undo it and the product's listeners hear it.
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
const SLOTS = join(ROOT, 'code_blocks/drivers/button/slots')

function run(scenario, restore = '') {
  const cfg = { gpio: 4, active_level: 0, toggle_before_reset: true, target_param: 'APP_DRIVER_PARAM_POWER', press_param: 'APP_DRIVER_PARAM_LED_PATTERN' }
  const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'B', prefix_lc: 'b', cfg })
  const prog = `#include "app_driver.h"
#include "esp_timer.h"
#include <cstdio>
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
#define BIT(n) (1UL << (n))
#define SOC_PM_SUPPORT_EXT1_WAKEUP 1
#define SOC_GPIO_SUPPORT_HP_PERIPH_PD_SLEEP_WAKEUP 1
static const char *TAG = "t";
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
typedef int gpio_num_t; typedef void *button_handle_t;
enum { BUTTON_SINGLE_CLICK, BUTTON_PRESS_DOWN, BUTTON_PRESS_UP, BUTTON_PRESS_END };
enum { ESP_RST_POWERON = 1, ESP_RST_DEEPSLEEP = 8 };
enum { ESP_SLEEP_WAKEUP_EXT1 = 3, ESP_SLEEP_WAKEUP_TIMER = 4, ESP_SLEEP_WAKEUP_GPIO = 7 };
static int g_reset = ESP_RST_POWERON; static uint32_t g_causes = 0; static uint64_t g_ext1 = 0, g_gpio = 0; static int g_level = 1;
static int esp_reset_reason(void) { return g_reset; }
static uint32_t esp_sleep_get_wakeup_causes(void) { return g_causes; }
static uint64_t esp_sleep_get_ext1_wakeup_status(void) { return g_ext1; }
static uint64_t esp_sleep_get_gpio_wakeup_status(void) { return g_gpio; }
static gpio_num_t esp_sleep_wakeup_io_bit2num(uint32_t b) { return (gpio_num_t)(b + 2); }   /* RTC IO n is GPIO n+2 here */
static int gpio_get_level(gpio_num_t) { return g_level; }
static button_handle_t app_button_get_or_create(int, int, int) { return (void *)1; }
static int iot_button_register_cb(button_handle_t, int, void *, void (*)(void *, void *), void *) { return 0; }
static int app_button_reset_hold_ms(button_handle_t) { return 0; }
static int g_events = 0;
static void count(app_driver_param_id_t id, app_driver_param_val_t, app_driver_handle_t, void *) { if (id == APP_DRIVER_PARAM_LED_PATTERN) g_events++; }
${fill('config_defines.h')}
${fill('logic_statics.c')}
int main() {
    app_driver_register_solution("t", count, nullptr);
    ${scenario}
    ${fill('logic_init.c')}
    ${restore}
    ${fill('logic_start.c')}
    app_driver_param_val_t v = {}; app_driver_get_param(APP_DRIVER_PARAM_POWER, &v);
    printf("%d %d\\n", (int)v.b, g_events);
    return 0;
}`
  const dir = mkdtempSync(join(tmpdir(), 'zc-btn-'))
  try {
    writeFileSync(join(dir, 'a.cpp'), prog)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-pthread', '-DAPP_DRIVER_POLL_NO_TASK', '-I', join(BUS, 'test/shims'), '-I', join(BUS, 'test'), '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'a.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    return execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim()
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
}

test('button: the press that woke the device is delivered once, as a click', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  // ext1: this pin (GPIO4) woke it, released by now (active low, reads 1)
  assert.equal(run('g_reset = ESP_RST_DEEPSLEEP; g_causes = BIT(ESP_SLEEP_WAKEUP_EXT1); g_ext1 = 1ULL << 4;'), '1 1')
  // deep-sleep GPIO wakeup reports RTC IO bits: bit 2 is GPIO4 here
  assert.equal(run('g_reset = ESP_RST_DEEPSLEEP; g_causes = BIT(ESP_SLEEP_WAKEUP_GPIO); g_gpio = 1ULL << 2;'), '1 1')
})

test('button: no extra press from a power-on, another pin, a timer, or a press still held', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  assert.equal(run(''), '0 0')
  assert.equal(run('g_reset = ESP_RST_DEEPSLEEP; g_causes = BIT(ESP_SLEEP_WAKEUP_EXT1); g_ext1 = 1ULL << 5;'), '0 0')
  assert.equal(run('g_reset = ESP_RST_DEEPSLEEP; g_causes = BIT(ESP_SLEEP_WAKEUP_TIMER);'), '0 0')
  assert.equal(run('g_reset = ESP_RST_DEEPSLEEP; g_causes = BIT(ESP_SLEEP_WAKEUP_EXT1); g_ext1 = 1ULL << 4; g_level = 0;'), '0 0')
})

test('button: a value restored at boot does not undo the press that woke the device', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  // nvs_persist restores POWER=on during the composed behaviors' init; the
  // wake press then turns it off, as a press would.
  const restore = 'app_driver_param_val_t r = {}; r.b = true; app_driver_set_param(APP_DRIVER_PARAM_POWER, r, APP_DRIVER_SOURCE_RESTORE);'
  assert.equal(run('g_reset = ESP_RST_DEEPSLEEP; g_causes = BIT(ESP_SLEEP_WAKEUP_EXT1); g_ext1 = 1ULL << 4;', restore), '0 1')
  assert.equal(run('', restore), '1 0')
})

test('logic_start slots run from zc_behaviors_start, which app_main calls after app_logic_init', async () => {
  const gen = await import(join(ROOT, 'engine/dist/generator.js'))
  const out = gen.genAppLogic([{ block: { id: 'drivers/button', kind: 'driver', slots: {} }, prefix: 'B', cfg: {},
    slots: { logic_init: '{ init_it(); }', logic_start: '{ start_it(); }' } }])
  const control = out.parts.find((p) => p.name === 'control.cpp').content
  assert.match(control, /static void zc_b_start\(void\)\n\{\n    start_it\(\); *\n\}/)
  assert.match(control, /void control_logic_start\(void\)\n\{\n    zc_b_start\(\);\n\}/)
  const entry = out.parts.find((p) => p.name === 'zc_behaviors.cpp').content
  assert.match(entry, /esp_err_t zc_behaviors_start\(void\)\n\{\n    control_logic_start\(\);/)
  const main = readFileSync(join(ROOT, 'base_firmware/main/app_main.cpp'), 'utf8')
  assert.ok(main.indexOf('zc_behaviors_start()') > main.indexOf('app_logic_init()'), 'started after the product logic')
  // without any logic_start, the entry point is still defined (app_main calls it)
  const none = gen.genAppLogic([]).parts.find((p) => p.name === 'zc_behaviors.cpp').content
  assert.match(none, /esp_err_t zc_behaviors_start\(void\)\n\{\n    return ESP_OK;/)
})
