// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/gpio_input_sensor: a level is published only once it has been SEEN
// to hold for debounce_ms. A one-poll glitch (contact bounce) is never
// published, also when debounce_ms is no longer than the poll period.
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
const SLOTS = join(ROOT, 'code_blocks/drivers/gpio_input_sensor/slots')

const HARNESS = (defines, statics) => String.raw`
#include <cstdio>
#include <string>
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
typedef int esp_err_t;
#define ESP_OK 0
static const char *TAG = "t";
typedef int gpio_num_t;
static int g_level = 1;
static int gpio_get_level(gpio_num_t) { return g_level; }
union app_driver_param_val_t { bool b; unsigned char u8; };
#define APP_DRIVER_SOURCE_LOCAL 0
enum { P_IN };
static bool g_param = false;
static int g_publishes = 0;
static esp_err_t app_driver_set_param(int, app_driver_param_val_t v, int) { if (v.b != g_param) g_publishes++; g_param = v.b; return ESP_OK; }
static esp_err_t app_driver_get_param(int, app_driver_param_val_t *v) { v->b = g_param; return ESP_OK; }
${defines}
${statics}
static std::string out;
static void poll(int level) { g_level = level; in_input_poll_cb(nullptr); }
int main()
{
    poll(1);                                  /* boot: cleared (active low) */
    poll(0); poll(1); poll(1);                /* a one-poll glitch */
    out += std::to_string(g_publishes) + " ";
    poll(0); poll(0); poll(0);                /* held */
    out += std::to_string(g_param) + " ";
    printf("%s\n", out.c_str());
    return 0;
}
`

for (const [poll, debounce] of [[30, 30], [250, 20], [10, 50]]) {
  test(`gpio_input_sensor poll ${poll} ms / debounce ${debounce} ms: a one-poll glitch is not published, a held level is`, { skip: !haveCxx && 'no host C++ compiler' }, () => {
    const cfg = { gpio: 4, pull: 'pullup', active_level: 0, poll_interval_ms: poll, debounce_ms: debounce, target_param: 'P_IN' }
    const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'IN', prefix_lc: 'in', cfg })
    const dir = mkdtempSync(join(tmpdir(), 'zc-in-'))
    try {
      writeFileSync(join(dir, 'main.cpp'), HARNESS(fill('config_defines.h'), fill('logic_statics.c')))
      execFileSync(CXX, ['-std=c++20', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
      const polls = Math.ceil(debounce / poll) + 1
      if (polls <= 3) assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '0 1')
      else assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim().split(' ')[0], '0', 'the glitch is not published')
    } finally {
      rmSync(dir, { recursive: true, force: true })
    }
  })
}
