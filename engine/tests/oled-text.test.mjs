// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/oled_ssd1306: text at any scale lands in the frame where asked,
// redrawing a value in place leaves no old pixels, and only _show (or the
// self-flushing _print/_clear) talks to the panel.
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
const SLOTS = join(ROOT, 'code_blocks/drivers/oled_ssd1306/slots')
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'),
  { prefix: 'OLED', prefix_lc: 'oled', cfg: { i2c_port: 0, i2c_address: 60, i2c_freq_hz: 400000 } })

const HARNESS = (defines, statics) => String.raw`
#include <cstdint>
#include <cstdio>
#include <cstring>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
static const char *TAG = "t";
static const char *esp_err_to_name(esp_err_t) { return "e"; }
typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
typedef int i2c_port_num_t;
struct i2c_device_config_t { int dev_addr_length; uint16_t device_address; uint32_t scl_speed_hz; };
enum { I2C_ADDR_BIT_LEN_7 };
static esp_err_t i2c_master_get_bus_handle(i2c_port_num_t, i2c_master_bus_handle_t *b) { *b = (void *)1; return ESP_OK; }
static esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t, const i2c_device_config_t *, i2c_master_dev_handle_t *d) { *d = (void *)1; return ESP_OK; }
static int g_tx = 0;
static esp_err_t i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t *, size_t, int) { g_tx++; return ESP_OK; }
typedef void *SemaphoreHandle_t;
#define portMAX_DELAY 0xffffffffu
static SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) { return (void *)1; }
static int xSemaphoreTakeRecursive(SemaphoreHandle_t, uint32_t) { return 1; }
static int xSemaphoreGiveRecursive(SemaphoreHandle_t) { return 1; }
${defines}
${statics}
static int lit(int x0, int y0, int w, int h)
{
    int n = 0;
    for (int y = y0; y < y0 + h; ++y) for (int x = x0; x < x0 + w; ++x)
        n += (s_oled_oled_fb[(y >> 3) * 128 + x] >> (y & 7)) & 1;
    return n;
}
int main()
{
    s_oled_oled_lock = xSemaphoreCreateRecursiveMutex();
    oled_i2c_attach();
    int glyph1 = 0;
    oled_oled_text(0, 0, 1, "8"); glyph1 = lit(0, 0, 6, 8);
    memset(s_oled_oled_fb, 0, sizeof(s_oled_oled_fb));
    int tx0 = g_tx;
    oled_oled_text(10, 20, 3, "8");
    int big = lit(10, 20, 18, 24), outside = lit(0, 0, 128, 64) - big, tx_draw = g_tx - tx0;
    oled_oled_text(10, 20, 3, "1");
    int one = lit(10, 20, 18, 24);
    oled_oled_text(10, 20, 3, "1");
    int again = lit(10, 20, 18, 24);
    oled_oled_show();
    printf("%d %d %d %d %d %d %d\n", glyph1, big, outside, tx_draw, one, again, g_tx - tx0);
    return 0;
}
`

test('oled_ssd1306 text: scaled glyphs, in-place redraw, one transfer per show', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-oled-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), HARNESS(fill('config_defines.h'), fill('driver_statics.c')))
    execFileSync(CXX, ['-std=c++20', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    const [glyph1, big, outside, txDraw, one, again, txShow] = execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim().split(' ').map(Number)
    assert.ok(glyph1 > 10, `an 8 at scale 1 lights ${glyph1} pixels`)
    assert.equal(big, glyph1 * 9, 'scale 3 lights every pixel as a 3x3 block')
    assert.equal(outside, 0, 'nothing outside the character cell')
    assert.equal(txDraw, 0, 'drawing alone sends nothing to the panel')
    assert.ok(one > 0 && one < big, 'a 1 drawn over the 8 replaces it')
    assert.equal(again, one, 'redrawing in place is stable')
    assert.equal(txShow, 8 * 4, 'one show = 8 pages, each 3 commands + 1 data packet')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
