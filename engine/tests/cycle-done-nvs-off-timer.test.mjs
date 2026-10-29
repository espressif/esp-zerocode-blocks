// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/cycle_done_detector: the 1 s tick runs on the shared esp_timer
// task and never touches flash; the bus's poll task writes the running state.
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/cycle_done_detector/slots')
const cfg = {
  activity_param: 'APP_DRIVER_PARAM_LEVEL', running_param: 'APP_DRIVER_PARAM_POWER', done_param: 'APP_DRIVER_PARAM_LED_PATTERN',
  min_level: 10, margin: 5, idle_level: 0, window_seconds: 10, start_active_seconds: 3, busy_active_seconds: 2,
  min_run_minutes: 0, quiet_minutes: 1, nvs_namespace: 'app', nvs_key: 'cyc', resume_after_reboot: 'false',
}
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'CY', prefix_lc: 'cy', cfg })

test('cycle_done_detector: the tick never writes flash; the poll task writes the last state', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const harness = String.raw`
#include "app_driver.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"
#include <cstdio>
#include <cstring>
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_ERROR_CHECK(x) ((void)(x))
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
static const char *TAG = "t";
typedef void *esp_timer_handle_t;
enum { ESP_TIMER_TASK };
struct esp_timer_create_args_t { void (*callback)(void *); void *arg; int dispatch_method; const char *name; bool skip_unhandled_events; };
static esp_err_t esp_timer_create(const esp_timer_create_args_t *, esp_timer_handle_t *h) { *h = (void *)1; return ESP_OK; }
static esp_err_t esp_timer_start_periodic(esp_timer_handle_t, uint64_t) { return ESP_OK; }
typedef uint32_t nvs_handle_t;
enum { NVS_READONLY, NVS_READWRITE };
static int g_writes = 0; static uint8_t g_stored = 9;
static esp_err_t nvs_open(const char *, int, nvs_handle_t *h) { *h = 1; return ESP_OK; }
static esp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *) { return -1; }
static esp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t v) { g_writes++; g_stored = v; return ESP_OK; }
static esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
static void nvs_close(nvs_handle_t) {}
${fill('config_defines.h')}
${fill('logic_statics.c')}
int main()
{
    ${fill('logic_init.c')}
    app_driver_param_val_t v = {}; v.u16 = 100;
    app_driver_set_param(APP_DRIVER_PARAM_LEVEL, v, APP_DRIVER_SOURCE_LOCAL);
    for (int i = 0; i < 5; i++) cy_cyc_tick_cb(NULL);          /* the cycle starts */
    int during = g_writes;
    zc_test_now_us = 2000000; app_driver_poll_step(zc_test_now_us);
    zc_test_now_us = 4000000; app_driver_poll_step(zc_test_now_us);
    printf("%d %d %d %u\n", s_cy_cyc_running ? 1 : 0, during, g_writes, g_stored);
    return 0;
}
`
  const dir = mkdtempSync(join(tmpdir(), 'zc-cyc-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), harness)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-pthread', '-DAPP_DRIVER_POLL_NO_TASK', '-I', join(BUS, 'test/shims'), '-I', join(BUS, 'test'), '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'main.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '1 0 1 1')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
