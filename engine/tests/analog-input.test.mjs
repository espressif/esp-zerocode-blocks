// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/analog_input: calibrated mV, the divider, the straight-line map (not
// clamped, so a fault shows), the average, and the status — sampled on the
// bus's poll step. Writers converted raw counts with a fixed 3100/4095 and
// judged a battery cut-off and a pressure alarm on readings 100-200 mV off.
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
const SLOTS = join(ROOT, 'code_blocks/drivers/analog_input/slots')

const TYPES = `#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef enum { APP_DRIVER_PARAM_NONE = 0, APP_DRIVER_PARAM_V, APP_DRIVER_PARAM_OK, APP_DRIVER_PARAM_MAX } app_driver_param_id_t;
typedef union { bool b; uint8_t u8; int16_t i16; uint16_t u16; uint32_t u32; } app_driver_param_val_t;
#define APP_DRIVER_MAX_SOLUTIONS 8
typedef uint8_t app_driver_handle_t;
#define APP_DRIVER_SOURCE_LOCAL 0
typedef void (*app_driver_notify_cb_t)(app_driver_param_id_t, app_driver_param_val_t, app_driver_handle_t, void *);
`
// The ADC: raw counts the test sets; "calibration" turns raw into mV = raw/2.
const STUBS = `
typedef void *adc_oneshot_unit_handle_t; typedef int adc_channel_t; typedef int adc_unit_t; typedef void *adc_cali_handle_t; typedef int adc_atten_t;
#define SOC_ADC_PERIPH_NUM 2
#define ADC_UNIT_1 0
#define ADC_BITWIDTH_DEFAULT 0
#define ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED 1
struct adc_oneshot_unit_init_cfg_t { adc_unit_t unit_id; };
struct adc_oneshot_chan_cfg_t { adc_atten_t atten; int bitwidth; };
struct adc_cali_curve_fitting_config_t { adc_unit_t unit_id; adc_channel_t chan; adc_atten_t atten; int bitwidth; };
static int g_raw = 0; static bool g_fail = false;
static esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *, adc_oneshot_unit_handle_t *h) { *h = (void *)1; return ESP_OK; }
static esp_err_t adc_oneshot_io_to_channel(int, adc_unit_t *u, adc_channel_t *c) { *u = 0; *c = 3; return ESP_OK; }
static esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t, adc_channel_t, const adc_oneshot_chan_cfg_t *) { return ESP_OK; }
static esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t, adc_channel_t, int *raw) { if (g_fail) return -1; *raw = g_raw; return ESP_OK; }
static esp_err_t adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t *, adc_cali_handle_t *h) { *h = (void *)1; return ESP_OK; }
static esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t, int raw, int *mv) { *mv = raw / 2; return ESP_OK; }
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
static const char *TAG = "t";
`

function run(cfg, body) {
  const full = { gpio: 4, attenuation: 3, divider_num: 1, divider_den: 1, in_lo_mv: 0, in_hi_mv: 3300, out_lo: 0, out_hi: 0, samples: 4, poll_ms: 1000, value_param: 'APP_DRIVER_PARAM_V', value_kind: 'i16', status_param: 'APP_DRIVER_PARAM_OK', ...cfg }
  const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'A', prefix_lc: 'a', cfg: full })
  const prog = `#include "app_driver.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstdlib>
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
${STUBS}
${fill('config_defines.h')}
${fill('driver_statics.c')}
static int val(void) { app_driver_param_val_t x = {}; app_driver_get_param(APP_DRIVER_PARAM_V, &x); return x.i16; }
static int ok(void) { app_driver_param_val_t x = {}; app_driver_get_param(APP_DRIVER_PARAM_OK, &x); return x.b; }
static void tick(void) { zc_test_now_us += 1000000; app_driver_poll_step(zc_test_now_us); }
int main() {
    ${fill('driver_init.c')}
    ${body}
    return 0;
}`
  const dir = mkdtempSync(join(tmpdir(), 'zc-ain-'))
  try {
    writeFileSync(join(dir, 'app_driver_types.h'), TYPES)
    writeFileSync(join(dir, 'a.cpp'), prog)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-pthread', '-DAPP_DRIVER_POLL_NO_TASK', '-I', dir, '-I', join(BUS, 'test/shims'), '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'a.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    return execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim()
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
}

test('analog_input: calibrated mV through a divider', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  // raw 3600 -> 1800 mV at the pin -> a 1:1 divider puts 3600 mV on the battery
  assert.equal(run({ divider_num: 2 }, `g_raw = 3600; tick(); printf("%d %d\\n", val(), ok());`), '3600 1')
})

test('analog_input: the straight-line map, unclamped', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  // 0.5-4.5 V -> 0-100 PSI behind a 2/1 divider: pin 1250 mV = 2500 mV = 50 PSI;
  // a broken transducer at 0 V reads -13 PSI, not a believable 0.
  const map = { divider_num: 2, in_lo_mv: 500, in_hi_mv: 4500, out_lo: 0, out_hi: 100 }
  assert.equal(run(map, `g_raw = 2500; tick(); printf("%d ", val()); g_raw = 0; tick(); printf("%d\\n", val());`), '50 -13')
})

test('analog_input: a failed read clears the status and keeps the last value', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  assert.equal(run({}, `g_raw = 2000; tick(); g_fail = true; tick(); printf("%d %d\\n", val(), ok());`), '1000 0')
})
