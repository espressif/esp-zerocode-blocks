// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// A sensor's first reading is a real one, soon after boot: drivers/bh1750
// waits out its first conversion instead of publishing the empty register
// (0 lux), and drivers/dht22 reads 2 s after start, not one whole (possibly
// minutes-long) poll period later.
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
const fill = (block, f, prefix, cfg) => eng.__substituteForTest(readFileSync(join(ROOT, 'code_blocks/drivers', block, 'slots', f), 'utf8'), { prefix, prefix_lc: prefix.toLowerCase(), cfg })
const run = (src) => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-first-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), src)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    return execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim()
  } finally { rmSync(dir, { recursive: true, force: true }) }
}

const COMMON = String.raw`
#include <cstdio>
#include <cstdint>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGD(tag, ...) ((void)(tag))
#define ESP_ERROR_CHECK(x) (void)(x)
static const char *TAG = "t";
static const char *esp_err_to_name(esp_err_t) { return "e"; }
static int64_t g_now_ms = 0;
#define pdMS_TO_TICKS(x) ((uint32_t)(x))
static void vTaskDelay(uint32_t t) { g_now_ms += t; }
union app_driver_param_val_t { bool b; uint8_t u8; int16_t i16; uint16_t u16; uint32_t u32; };
#define APP_DRIVER_SOURCE_LOCAL 0
`

test('bh1750: the first published lux is the first conversion, not the empty register', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const cfg = { i2c_freq_hz: 100000, i2c_port: 0, i2c_address: 35, poll_interval_ms: 1000, value_param: 'P_LUX', status_param: '' }
  const out = run(COMMON + String.raw`
typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
typedef int i2c_port_num_t;
struct i2c_device_config_t { int dev_addr_length; uint16_t device_address; uint32_t scl_speed_hz; };
enum { I2C_ADDR_BIT_LEN_7 };
static esp_err_t i2c_master_get_bus_handle(i2c_port_num_t, i2c_master_bus_handle_t *b) { *b = (void *)1; return ESP_OK; }
static esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t, const i2c_device_config_t *, i2c_master_dev_handle_t *d) { *d = (void *)1; return ESP_OK; }
static int64_t g_armed_at = -1;
static esp_err_t i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t *, size_t, int) { g_armed_at = g_now_ms; return ESP_OK; }
static esp_err_t i2c_master_receive(i2c_master_dev_handle_t, uint8_t *b, size_t, int)
{   /* 600 raw = 500 lux, ready 120-180 ms after the arm; 0 before. */
    uint16_t raw = (g_now_ms - g_armed_at >= 180) ? 600 : 0; b[0] = raw >> 8; b[1] = raw & 0xFF; return ESP_OK; }
enum { P_LUX };
static int g_first = -1;
static int app_driver_set_param(int, app_driver_param_val_t v, int) { if (g_first < 0) g_first = v.u16; return 0; }
typedef void (*app_driver_poll_fn_t)(void *);
static app_driver_poll_fn_t g_cb;
static esp_err_t app_driver_poll_every(app_driver_poll_fn_t fn, void *, uint32_t) { g_cb = fn; return ESP_OK; }
${fill('bh1750', 'config_defines.h', 'LUX', cfg)}
${fill('bh1750', 'logic_statics.c', 'LUX', cfg)}
int main() { ${fill('bh1750', 'logic_init.c', 'LUX', cfg)} g_cb(nullptr); printf("%d\n", g_first); return 0; }
`)
  assert.equal(out, '500')
})

test('dht22: the first reading is 2 s after start, not one poll period later', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const cfg = { gpio: 4, sensor_variant: 'dht22', is_dht11: 0, poll_interval_ms: 120000, temp_param: 'P_T', humidity_param: 'P_H', status_param: '' }
  const src = fill('dht22', 'logic_statics.c', 'DHT', cfg)
  const task = src.slice(src.indexOf('static void dht_dht_task(void *arg)'))
  const out = run(COMMON + String.raw`
typedef uint32_t TickType_t;
static TickType_t xTaskGetTickCount(void) { return (TickType_t)g_now_ms; }
static int64_t g_first_poll_ms = -1;
static int g_polls = 0;
static void vTaskDelayUntil(TickType_t *last, uint32_t inc) { *last += inc; g_now_ms = *last; if (g_polls >= 2) throw 0; }
static void dht_dht_poll(void) { if (g_first_poll_ms < 0) g_first_poll_ms = g_now_ms; g_polls++; }
${fill('dht22', 'config_defines.h', 'DHT', cfg)}
${task}
int main() { try { dht_dht_task(nullptr); } catch (int) {} printf("%lld\n", (long long)g_first_poll_ms); return 0; }
`)
  assert.equal(out, '2000')
})
