// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/factory_reset_power_cycle: when the factory reset's erase fails
// (app_utils_factory_reset returns instead of rebooting), nothing is written
// through the NVS handle the block already closed; below the threshold the
// count is stored as before.
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/factory_reset_power_cycle/slots')
const cfg = { count_threshold: 5, stable_window_ms: 5000, nvs_namespace: 'app', nvs_key: 'pc_count' }
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'FR', prefix_lc: 'fr', cfg })

const HARNESS = (defines, statics, init) => String.raw`
#include <cstdint>
#include <cstdio>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERROR_CHECK(x) (void)(x)
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
static const char *TAG = "t";
typedef uint32_t nvs_handle_t;
enum { NVS_READWRITE };
static uint8_t g_stored = 4;          /* one short of the threshold */
static bool g_open = false;
static int g_closed_use = 0, g_resets = 0;
static esp_err_t nvs_open(const char *, int, nvs_handle_t *h) { g_open = true; *h = 1; return ESP_OK; }
static esp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *v) { if (!g_open) g_closed_use++; *v = g_stored; return ESP_OK; }
static esp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t v) { if (!g_open) { g_closed_use++; return ESP_FAIL; } g_stored = v; return ESP_OK; }
static esp_err_t nvs_commit(nvs_handle_t) { if (!g_open) g_closed_use++; return ESP_OK; }
static void nvs_close(nvs_handle_t) { if (!g_open) g_closed_use++; g_open = false; }
static esp_err_t nvs_erase_key(nvs_handle_t, const char *) { return ESP_OK; }
static esp_err_t app_utils_factory_reset(void) { g_resets++; return ESP_FAIL; }   /* the erase failed */
typedef int esp_reset_reason_t;
#define ESP_RST_POWERON 1
#define ESP_RST_EXT 2
static esp_reset_reason_t esp_reset_reason(void) { return ESP_RST_POWERON; }
typedef void (*esp_timer_cb_t)(void *);
enum esp_timer_dispatch_t { ESP_TIMER_TASK };
struct esp_timer_create_args_t { esp_timer_cb_t callback; void *arg; esp_timer_dispatch_t dispatch_method; const char *name; bool skip_unhandled_events; };
typedef void *esp_timer_handle_t;
static esp_err_t esp_timer_create(const esp_timer_create_args_t *, esp_timer_handle_t *h) { *h = (void *)1; return ESP_OK; }
static esp_err_t esp_timer_start_once(esp_timer_handle_t, uint64_t) { return ESP_OK; }
${defines}
${statics}
int main()
{
    ${init}
    printf("%d %d\n", g_resets, g_closed_use);
    return 0;
}
`

test('factory_reset_power_cycle: a failed erase writes nothing through the closed handle', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-frpc-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), HARNESS(fill('config_defines.h'), fill('logic_statics.c'), fill('logic_init.c')))
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '1 0', 'one reset attempt, no use of the closed handle')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
