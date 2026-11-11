// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/sensor_filter: the window is SAMPLED on the poll task, so a steady
// reading (whose writes the bus does not notify) still fills it — a window fed
// from change notifications froze on a load that settled at 0. Median rejects
// an outlier, a stale source adds nothing, negatives round away from zero.
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/sensor_filter/slots')

const TYPES = `#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef enum { APP_DRIVER_PARAM_NONE = 0, APP_DRIVER_PARAM_SRC, APP_DRIVER_PARAM_OUT, APP_DRIVER_PARAM_MAX } app_driver_param_id_t;
typedef union { bool b; uint8_t u8; int16_t i16; uint16_t u16; uint32_t u32; } app_driver_param_val_t;
#define APP_DRIVER_MAX_SOLUTIONS 8
typedef uint8_t app_driver_handle_t;
#define APP_DRIVER_SOURCE_LOCAL 0
typedef void (*app_driver_notify_cb_t)(app_driver_param_id_t, app_driver_param_val_t, app_driver_handle_t, void *);
`

function run(cfg, body) {
  const full = { source_param: 'APP_DRIVER_PARAM_SRC', output_param: 'APP_DRIVER_PARAM_OUT', value_kind: 'i16', mode: 'average', samples: 5, sample_ms: 1000, stale_after_ms: 0, ...cfg }
  const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'F', prefix_lc: 'f', cfg: full })
  const prog = `#include "app_driver.h"
#include "esp_timer.h"
#include <cstdio>
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
static const char *TAG = "t";
${fill('config_defines.h')}
${fill('logic_statics.c')}
static void put(int16_t v) { app_driver_param_val_t x = {}; x.i16 = v; app_driver_set_param(APP_DRIVER_PARAM_SRC, x, APP_DRIVER_SOURCE_LOCAL); }
static int out(void) { app_driver_param_val_t x = {}; app_driver_get_param(APP_DRIVER_PARAM_OUT, &x); return x.i16; }
static void tick(void) { zc_test_now_us += 1000000; app_driver_poll_step(zc_test_now_us); }
int main()
{
    ${fill('logic_init.c')}
    ${body}
    return 0;
}
`
  const dir = mkdtempSync(join(tmpdir(), 'zc-flt-'))
  try {
    writeFileSync(join(dir, 'app_driver_types.h'), TYPES)
    writeFileSync(join(dir, 'main.cpp'), prog)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-pthread', '-DAPP_DRIVER_POLL_NO_TASK', '-I', dir, '-I', join(BUS, 'test/shims'), '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'main.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    return execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim()
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
}

test('sensor_filter: a steady reading keeps filling the window', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  // 100 for five periods, then the load settles at 0 and stays there.
  assert.equal(run({}, `
    for (int i = 0; i < 5; i++) { put(100); tick(); }
    printf("%d ", out());
    for (int i = 0; i < 2; i++) { put(0); tick(); }
    printf("%d ", out());
    for (int i = 0; i < 3; i++) { put(0); tick(); }
    printf("%d\\n", out());`), '100 60 0')
})

test('sensor_filter: median drops an outlier; negatives round away from zero', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  assert.equal(run({ mode: 'median', samples: 3 }, `put(20); tick(); put(500); tick(); put(21); tick(); printf("%d\\n", out());`), '21')
  assert.equal(run({ samples: 2 }, `put(-5); tick(); put(-6); tick(); printf("%d\\n", out());`), '-6')
})

test('sensor_filter: a stale source adds nothing', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  // Written once at t=0 (value 40); the window would otherwise keep sampling 40.
  assert.equal(run({ stale_after_ms: 2500, samples: 4 }, `
    put(10); tick(); put(40); tick(); tick(); tick(); tick(); tick();
    printf("%d\\n", out());`), '30')
})
