// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/sht3x, sht4x: a frame whose CRC does not match is refused, never
// published as a reading (a bus glitch read as 130 °C drives a heater).
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))
const CC = process.env.CC || 'cc'
const haveCc = spawnSync(CC, ['--version']).status === 0

// The read function (and the helper before it) out of the block's statics.
function readFn(block, name) {
  const slot = (f) => eng.__substituteForTest(readFileSync(join(ROOT, 'code_blocks/drivers', block, 'slots', f), 'utf8'), CTX)
  const src = slot('logic_statics.c')
  const defs = slot('config_defines.h')
  return defs + '\n' + pick(src, name)
}
const CTX = { prefix: 'S', prefix_lc: 's', cfg: { temp_param: 'P_T', humidity_param: 'P_H', status_param: 'P_OK', poll_interval_ms: 2000, i2c_port: 0, i2c_address: 68, i2c_freq_hz: 100000, i2c_sda: 4, i2c_scl: 5, precision: 'high', heater: 'off' } }
function pick(src, name) {
  const start = src.indexOf('/* Sensirion CRC-8')
  const end = src.indexOf('\nstatic void s_' + name + '_poll_cb')
  return src.slice(start >= 0 ? start : src.indexOf('static esp_err_t s_' + name + '_read'), end)
}

const STUBS = `
#include <stdint.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_CRC 0x109
#define configTICK_RATE_HZ 100
#define pdMS_TO_TICKS(x) (x)
static void vTaskDelay(int t) { (void)t; }
typedef void *i2c_master_dev_handle_t;
static i2c_master_dev_handle_t s_s_i2c_dev = 0;
static uint8_t g_frame[6];
static esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d, const uint8_t *b, int n, int t) { (void)d; (void)b; (void)n; (void)t; return ESP_OK; }
static esp_err_t i2c_master_receive(i2c_master_dev_handle_t d, uint8_t *b, int n, int t) { (void)d; (void)t; for (int i = 0; i < n; i++) b[i] = g_frame[i]; return ESP_OK; }
`

for (const [block, name] of [['sht3x', 'sht3'], ['sht4x', 'sht4x']]) {
  test(`${block}: a good frame reads, a corrupted one is refused`, { skip: !haveCc && 'no host C compiler' }, () => {
    const prog = STUBS + readFn(block, name) + `
int main(void)
{
    /* Sensirion's datasheet example: word 0xBEEF has CRC 0x92. */
    const uint8_t good[6] = { 0xBE, 0xEF, 0x92, 0xBE, 0xEF, 0x92 };
    int16_t t = 0, h = 0;
    for (int i = 0; i < 6; i++) g_frame[i] = good[i];
    esp_err_t a = s_${name}_read(&t, &h);
    g_frame[1] ^= 0x01;                        /* one bit flipped on the bus */
    esp_err_t b = s_${name}_read(&t, &h);
    printf("%d %d\\n", a, b);
    return 0;
}
`
    const dir = mkdtempSync(join(tmpdir(), 'zc-crc-'))
    try {
      writeFileSync(join(dir, 'a.c'), prog)
      execFileSync(CC, ['-std=c11', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'a.c')], { stdio: 'pipe' })
      assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), `0 ${0x109}`)
    } finally {
      rmSync(dir, { recursive: true, force: true })
    }
  })
}
