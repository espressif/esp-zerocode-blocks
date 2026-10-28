// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/imu_bmi270 against a simulated BMI270: one that answers only after
// boot is picked up, and one that is power-cycled (forgetting its +/-2 g
// configuration) is configured again — it used to probe once at boot and give
// up for good.
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
const SLOTS = join(ROOT, 'code_blocks/drivers/imu_bmi270/slots')
const cfg = { i2c_port: 0, i2c_address: 0x68, i2c_freq_hz: 400000, poll_interval_ms: 100, motion_threshold_mg: 150, motion_hold_ms: 2000, motion_param: 'P_MOTION', orientation_param: '' }
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'IMU', prefix_lc: 'imu', cfg })

const HARNESS = (defines, statics, init) => String.raw`
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
static const char *TAG = "t";
static const char *esp_err_to_name(esp_err_t) { return "e"; }
static void vTaskDelay(int) {}
#define pdMS_TO_TICKS(x) (x)
static void esp_rom_delay_us(int) {}
static int64_t esp_timer_get_time(void) { static int64_t t = 0; return t += 100000; }
typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
typedef int i2c_port_num_t;
struct i2c_device_config_t { int dev_addr_length; uint16_t device_address; uint32_t scl_speed_hz; };
enum { I2C_ADDR_BIT_LEN_7 };
static esp_err_t i2c_master_get_bus_handle(i2c_port_num_t, i2c_master_bus_handle_t *b) { *b = (void *)1; return ESP_OK; }
static esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t, const i2c_device_config_t *, i2c_master_dev_handle_t *d) { *d = (void *)1; return ESP_OK; }
/* The simulated BMI270: registers survive only while powered. */
static bool g_powered = false;
static uint8_t g_reg[256];
static void power_cycle(bool on) { g_powered = on; for (int i = 0; i < 256; i++) g_reg[i] = 0; g_reg[0x41] = 0x02; /* reset: +/-8 g */ }
static esp_err_t i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t *b, size_t n, int) { if (!g_powered) return ESP_FAIL; if (n == 2) g_reg[b[0]] = b[1]; return ESP_OK; }
static esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t, const uint8_t *w, size_t, uint8_t *r, size_t n, int)
{
    if (!g_powered) return ESP_FAIL;
    uint8_t reg = w[0];
    if (reg == 0x00) { r[0] = 0x24; return ESP_OK; }
    if (reg == 0x0C) { int16_t z = 16384; uint8_t d[6] = { 0, 0, 0, 0, (uint8_t)(z & 0xFF), (uint8_t)(z >> 8) }; for (size_t i = 0; i < n; i++) r[i] = d[i]; return ESP_OK; }
    for (size_t i = 0; i < n; i++) r[i] = g_reg[reg + i];
    return ESP_OK;
}
union app_driver_param_val_t { bool b; uint8_t u8; uint16_t u16; };
enum { P_MOTION };
#define APP_DRIVER_SOURCE_LOCAL 0
static int app_driver_set_param(int, app_driver_param_val_t, int) { return 0; }
typedef void (*app_driver_poll_fn_t)(void *);
static app_driver_poll_fn_t g_cb;
static esp_err_t app_driver_poll_every(app_driver_poll_fn_t fn, void *, uint32_t) { g_cb = fn; return ESP_OK; }
${defines}
${statics}
static std::string out;
static void polls(int n) { for (int i = 0; i < n; i++) g_cb(nullptr); out += std::to_string(s_imu_present) + (g_reg[0x41] == 0x00 ? "c" : "-") + " "; }
int main()
{
    power_cycle(false);
    ${init}
    polls(3);                        /* absent at boot */
    power_cycle(true); polls(1);     /* appears: probed and configured */
    polls(2);                        /* steady */
    power_cycle(false); polls(3);    /* gone: back to probing */
    power_cycle(true); polls(1);     /* back, unconfigured: configured again */
    printf("%s\n", out.c_str());
    return 0;
}
`

test('imu_bmi270: absent at boot is picked up later; a power-cycled part is configured again', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-imu-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), HARNESS(fill('config_defines.h'), fill('driver_statics.c'), fill('driver_init.c')))
    execFileSync(CXX, ['-std=c++20', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '0- 1c 1c 0- 1c')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
