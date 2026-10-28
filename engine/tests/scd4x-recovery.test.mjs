// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/scd4x against a simulated sensor: one missing at boot and plugged in
// later, and one that loses power mid-run, both read again by themselves.
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
const SLOTS = join(ROOT, 'code_blocks/drivers/scd4x/slots')
const cfg = { i2c_freq_hz: 100000, i2c_port: 0, i2c_address: 0x62, poll_interval_ms: 6000, co2_param: 'P_CO2', air_quality_param: 'P_AQ', temp_param: '', humidity_param: '' }
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'SCD', prefix_lc: 'scd', cfg })

const HARNESS = (defines, statics, init) => String.raw`
#include <cstdint>
#include <cstdio>
#include <string>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_ERROR_CHECK(x) (void)(x)
static const char *TAG = "t";
static const char *esp_err_to_name(esp_err_t) { return "e"; }
static void vTaskDelay(int) {}
#define pdMS_TO_TICKS(x) (x)
static void esp_rom_delay_us(int) {}
typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
typedef int i2c_port_num_t;
struct i2c_device_config_t { int dev_addr_length; uint16_t device_address; uint32_t scl_speed_hz; };
enum { I2C_ADDR_BIT_LEN_7 };
static esp_err_t i2c_master_get_bus_handle(i2c_port_num_t, i2c_master_bus_handle_t *b) { *b = (void *)1; return ESP_OK; }
static esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t, const i2c_device_config_t *, i2c_master_dev_handle_t *d) { *d = (void *)1; return ESP_OK; }
/* The simulated sensor. */
static bool g_powered = false, g_measuring = false, g_asked = false;
static esp_err_t i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t *b, size_t, int)
{
    if (!g_powered) return ESP_FAIL;
    uint16_t cmd = (uint16_t)(b[0] << 8 | b[1]);
    if (cmd == 0x21B1) { if (g_measuring) return ESP_FAIL; g_measuring = true; }
    if (cmd == 0x3F86) g_measuring = false;
    if (cmd == 0xEC05) { if (!g_measuring) return ESP_FAIL; g_asked = true; }
    return ESP_OK;
}
static uint8_t crc8(const uint8_t *w) { uint8_t c = 0xFF; for (int i = 0; i < 2; i++) { c ^= w[i]; for (int k = 0; k < 8; k++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1); } return c; }
static esp_err_t i2c_master_receive(i2c_master_dev_handle_t, uint8_t *b, size_t, int)
{
    if (!g_powered || !g_asked) return ESP_FAIL;
    g_asked = false;
    uint8_t w[9] = { 0x02, 0x58, 0, 0x66, 0x66, 0, 0x80, 0x00, 0 };   /* 600 ppm */
    for (int i = 0; i < 9; i += 3) w[i + 2] = crc8(&w[i]);
    for (int i = 0; i < 9; i++) b[i] = w[i];
    return ESP_OK;
}
union app_driver_param_val_t { bool b; uint8_t u8; int16_t i16; uint16_t u16; uint32_t u32; };
enum { P_CO2, P_AQ };
#define APP_DRIVER_SOURCE_LOCAL 0
static uint16_t g_co2 = 0;
static int app_driver_set_param(int id, app_driver_param_val_t v, int) { if (id == P_CO2) g_co2 = v.u16; return 0; }
/* The poll task: the test runs the registered poll by hand. */
typedef void (*app_driver_poll_fn_t)(void *);
static app_driver_poll_fn_t g_cb;
static esp_err_t app_driver_poll_every(app_driver_poll_fn_t fn, void *, uint32_t) { g_cb = fn; return ESP_OK; }
${defines}
${statics}
static std::string out;
static void polls(int n) { for (int i = 0; i < n; i++) g_cb(nullptr); out += std::to_string(g_co2) + " "; g_co2 = 0; }
int main()
{
    ${init}
    polls(2);                               /* absent at boot: nothing */
    g_powered = true; polls(4);             /* plugged in (idle): restarted, reading again */
    polls(1);                               /* steady */
    g_powered = false; g_measuring = false; polls(2);   /* power lost */
    g_powered = true; polls(4);             /* back: restarted again */
    printf("%s\n", out.c_str());
    return 0;
}
`

test('scd4x: a sensor absent at boot, or power-cycled, reads again by itself', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-scd-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), HARNESS(fill('config_defines.h'), fill('logic_statics.c'), fill('logic_init.c')))
    execFileSync(CXX, ['-std=c++20', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '0 600 600 0 600')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
