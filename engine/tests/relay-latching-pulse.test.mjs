// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/relay_latching: a command returns without waiting out the coil
// pulse (it used to vTaskDelay on the sender's task), the pulse ends when its
// timer fires, and a command arriving mid-pulse never energises both coils.
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
const SLOTS = join(ROOT, 'code_blocks/drivers/relay_latching/slots')
const cfg = { set_gpio: 4, reset_gpio: 5, pulse_ms: 50, active_level: 1, param_id: 'P_POWER' }
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'LR', prefix_lc: 'lr', cfg })

const HARNESS = (defines, statics, init, apply) => String.raw`
#include <cstdio>
#include <cstdint>
#include <string>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERROR_CHECK(x) (void)(x)
#define ESP_LOGI(tag, ...) ((void)(tag))
static const char *TAG = "t";
typedef int gpio_num_t;
static int g_level[64];
static int g_both = 0, g_delays = 0;
static void gpio_set_level(gpio_num_t n, int l) { g_level[n] = l; if (g_level[4] == 1 && g_level[5] == 1) g_both++; }
enum { GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE = 0, GPIO_PULLDOWN_DISABLE = 0, GPIO_INTR_DISABLE = 0 };
struct gpio_config_t { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; };
static esp_err_t gpio_config(const gpio_config_t *) { return ESP_OK; }
struct portMUX_TYPE { int x; };
#define portMUX_INITIALIZER_UNLOCKED {0}
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
static void esp_rom_delay_us(uint32_t) { g_delays++; }
typedef void (*esp_timer_cb_t)(void *);
enum esp_timer_dispatch_t { ESP_TIMER_TASK };
struct esp_timer_create_args_t { esp_timer_cb_t callback; void *arg; esp_timer_dispatch_t dispatch_method; const char *name; bool skip_unhandled_events; };
struct zc_t { esp_timer_cb_t cb; bool armed; };
typedef zc_t *esp_timer_handle_t;
static zc_t g_t;
static esp_err_t esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *h) { g_t.cb = a->callback; *h = &g_t; return ESP_OK; }
static esp_err_t esp_timer_start_once(esp_timer_handle_t h, uint64_t) { h->armed = true; return ESP_OK; }
static esp_err_t esp_timer_stop(esp_timer_handle_t h) { if (!h->armed) return ESP_ERR_INVALID_STATE; h->armed = false; return ESP_OK; }
static void fire(void) { if (g_t.armed) { g_t.armed = false; g_t.cb(nullptr); } }
/* The timer expired and its callback is on its way: too late to stop it. */
static bool g_inflight = false;
static void expire(void) { if (g_t.armed) { g_t.armed = false; g_inflight = true; } }
static void deliver(void) { if (g_inflight) { g_inflight = false; g_t.cb(nullptr); } }
union app_driver_param_val_t { bool b; };
enum { P_POWER };
${defines}
${statics}
static esp_err_t apply(int id, app_driver_param_val_t val) { switch (id) { ${apply} default: return ESP_OK; } }
int main()
{
    ${init}
    std::string out;
    app_driver_param_val_t on = { true }, off = { false };
    apply(P_POWER, on);  out += std::to_string(g_level[4]) + std::to_string(g_level[5]) + " ";   /* set coil driven, returned at once */
    fire();              out += std::to_string(g_level[4]) + std::to_string(g_level[5]) + " ";   /* pulse ended */
    apply(P_POWER, on);  apply(P_POWER, off);                                                   /* a command mid-pulse */
    out += std::to_string(g_level[4]) + std::to_string(g_level[5]) + " ";
    fire();              out += std::to_string(g_level[4]) + std::to_string(g_level[5]) + " ";
    apply(P_POWER, on);  expire();                                                              /* its end is on its way... */
    apply(P_POWER, off); deliver();                                                             /* ...and lands after the next command */
    out += std::to_string(g_level[4]) + std::to_string(g_level[5]) + " ";                      /* the new pulse is still on */
    fire();              out += std::to_string(g_level[4]) + std::to_string(g_level[5]) + " ";
    printf("%s%d %d\n", out.c_str(), g_both, g_delays);
    return 0;
}
`

test('relay_latching: the pulse never blocks the sender, never drives both coils, and a late end of an earlier pulse never cuts the next one short', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-lr-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), HARNESS(fill('config_defines.h'), fill('driver_statics.c'), fill('driver_init.c'), fill('driver_apply_cases.c')))
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '10 00 01 00 01 00 0 0')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
