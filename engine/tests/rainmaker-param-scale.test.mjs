// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/rainmaker_param with scale: a param in hundredths shows in the app
// as the real value (60.00, not 6000), and an app write comes back to the
// param's units, rounded and clamped; scale 1 is unchanged.
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/rainmaker_param/slots')

const HARNESS = (defines, statics, write) => String.raw`
#include <cstdio>
#include <cstdint>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_LOGW(tag, ...) ((void)(tag))
static const char *TAG = "t";
enum { RMAKER_VAL_TYPE_INTEGER, RMAKER_VAL_TYPE_FLOAT, RMAKER_VAL_TYPE_STRING, RMAKER_VAL_TYPE_BOOLEAN };
struct esp_rmaker_param_val_t { int type; union { int i; float f; bool b; char *s; } val; };
static esp_rmaker_param_val_t esp_rmaker_int(int i) { esp_rmaker_param_val_t v{}; v.type = RMAKER_VAL_TYPE_INTEGER; v.val.i = i; return v; }
static esp_rmaker_param_val_t esp_rmaker_float(float f) { esp_rmaker_param_val_t v{}; v.type = RMAKER_VAL_TYPE_FLOAT; v.val.f = f; return v; }
static esp_rmaker_param_val_t esp_rmaker_bool(bool b) { esp_rmaker_param_val_t v{}; v.type = RMAKER_VAL_TYPE_BOOLEAN; v.val.b = b; return v; }
static esp_rmaker_param_val_t esp_rmaker_str(const char *s) { esp_rmaker_param_val_t v{}; v.type = RMAKER_VAL_TYPE_STRING; v.val.s = (char *)s; return v; }
typedef void esp_rmaker_device_t; typedef void esp_rmaker_param_t;
union app_driver_param_val_t { bool b; uint8_t u8; int16_t i16; uint16_t u16; uint32_t u32; };
enum { P_T };
static app_driver_param_val_t g_set{};
static int s_handle = 0;
static void app_driver_set_param(int, app_driver_param_val_t v, int) { g_set = v; }
static esp_rmaker_param_val_t g_rep{};
static void esp_rmaker_param_update_and_report(esp_rmaker_param_t *, esp_rmaker_param_val_t v) { g_rep = v; }
${defines}
${statics}
static esp_err_t write_cb(esp_rmaker_device_t *device, esp_rmaker_param_t *param, esp_rmaker_param_val_t val)
{
${write}
    return ESP_FAIL_UNHANDLED;
}
int main()
{
    s_t_rmp_device = (esp_rmaker_device_t *)1; s_t_rmp_param = (esp_rmaker_param_t *)1;
    app_driver_param_val_t v{}; v.i16 = 6000;
    esp_rmaker_param_val_t shown = t_rmp_val(v);
    printf("%d %.2f ", shown.type, shown.type == RMAKER_VAL_TYPE_FLOAT ? shown.val.f : (float)shown.val.i);
#if defined(T_RMP_SCALED)   /* the app writes the real value, a float or a whole number */
    write_cb(s_t_rmp_device, s_t_rmp_param, esp_rmaker_float(62.5f));  printf("%d ", g_set.i16);
    write_cb(s_t_rmp_device, s_t_rmp_param, esp_rmaker_int(70));       printf("%d ", g_set.i16);
    write_cb(s_t_rmp_device, s_t_rmp_param, esp_rmaker_float(99.0f));  printf("%d\n", g_set.i16);
#else                         /* an unscaled tile is an integer param */
    write_cb(s_t_rmp_device, s_t_rmp_param, esp_rmaker_int(6250));     printf("%d ", g_set.i16);
    write_cb(s_t_rmp_device, s_t_rmp_param, esp_rmaker_int(7000));     printf("%d ", g_set.i16);
    write_cb(s_t_rmp_device, s_t_rmp_param, esp_rmaker_int(9900));     printf("%d\n", g_set.i16);
#endif
    return 0;
}
`

const run = (scale) => {
  const cfg = { label: 'T', kind: 'slider', param: 'P_T', value_kind: 'i16', min: 4500, max: 8000, step: 50, scale }
  const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'T', prefix_lc: 't', cfg })
  const dir = mkdtempSync(join(tmpdir(), 'zc-rms-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), HARNESS(fill('config_defines.h'), fill('rainmaker_statics.c'), fill('rainmaker_write_cb_cases.c')).replace('ESP_FAIL_UNHANDLED', '-1'))
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    return execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim()
  } finally { rmSync(dir, { recursive: true, force: true }) }
}

test('rainmaker_param scale 100: 6000 shows as 60.00; 62.5 and 70 write 6250 and 7000; 99 clamps to 8000', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  assert.equal(run(100), '1 60.00 6250 7000 8000')
})
test('rainmaker_param scale 1: unchanged integers', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  assert.equal(run(1), '0 6000.00 6250 7000 8000')
})
