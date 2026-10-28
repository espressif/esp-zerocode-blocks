// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/nvs_persist_*: the param listener never touches flash (it runs on
// whichever task changed the param); the bus's poll task writes, once per
// burst, the last value.
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/nvs_persist_u16/slots')
const cfg = { target_param: 'APP_DRIVER_PARAM_LEVEL', nvs_namespace: 'app', nvs_key: 'level' }
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'PL', prefix_lc: 'pl', cfg })

test('nvs_persist_u16: a burst of changes is one write of the last value, off the writer task', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const harness = String.raw`
#include "app_driver.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"
#include <cstdio>
#define ESP_LOGI(tag, ...) ((void)(tag))
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
static const char *TAG = "t";
typedef uint32_t nvs_handle_t;
enum { NVS_READWRITE };
static int g_writes = 0; static uint16_t g_stored = 0;
static esp_err_t nvs_open(const char *, int, nvs_handle_t *h) { *h = 1; return ESP_OK; }
static esp_err_t nvs_get_u16(nvs_handle_t, const char *, uint16_t *) { return -1; }
static esp_err_t nvs_set_u16(nvs_handle_t, const char *, uint16_t v) { g_writes++; g_stored = v; return ESP_OK; }
static esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
static void nvs_close(nvs_handle_t) {}
${fill('config_defines.h')}
${fill('logic_statics.c')}
int main()
{
    ${fill('logic_init.c')}
    zc_test_now_us = 1000000;
    app_driver_poll_step(zc_test_now_us);                       /* nothing changed yet */
    for (int i = 1; i <= 10; i++) { app_driver_param_val_t v = {}; v.u16 = (uint16_t)(i * 100); app_driver_set_param(APP_DRIVER_PARAM_LEVEL, v, APP_DRIVER_SOURCE_LOCAL); }
    int during = g_writes;                                     /* the listener wrote nothing */
    zc_test_now_us += 250000; app_driver_poll_step(zc_test_now_us);
    zc_test_now_us += 250000; app_driver_poll_step(zc_test_now_us);
    printf("%d %d %u\n", during, g_writes, g_stored);
    return 0;
}
`
  const dir = mkdtempSync(join(tmpdir(), 'zc-nvs-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), harness)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-pthread', '-DAPP_DRIVER_POLL_NO_TASK', '-I', join(BUS, 'test/shims'), '-I', join(BUS, 'test'), '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'main.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '0 1 1000')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
