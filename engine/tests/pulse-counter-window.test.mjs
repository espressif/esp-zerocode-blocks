// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/pulse_counter: with rate_window_ms equal to publish_ms the previous
// sample sits on the window's edge, and a timer that fires a hair late pushed
// it out — the rate read 0 every other publish on a steady flow. A rate always
// spans at least the previous sample; a stopped input still reads 0.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))
const CC = process.env.CXX || 'c++'
const haveCc = spawnSync(CC, ['--version']).status === 0
const SLOTS = join(ROOT, 'code_blocks/drivers/pulse_counter/slots')

function run(cfg, body) {
  const full = { gpio: 4, pulses_per_unit: 1, rate_window_ms: 1000, publish_ms: 1000, min_interval_us: 0, count_param: '', rate_param: 'P_RATE', pull: 'pullup', edge: 'falling', ...cfg }
  const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'PC', prefix_lc: 'pc', cfg: full })
  const prog = `#include <cstdint>
#include <cstdio>
typedef int esp_err_t; typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) (void)(m)
#define portEXIT_CRITICAL(m) (void)(m)
#define portENTER_CRITICAL_ISR(m) (void)(m)
#define portEXIT_CRITICAL_ISR(m) (void)(m)
#define IRAM_ATTR
#define ESP_OK 0
static int64_t g_now = 0;
static int64_t esp_timer_get_time(void) { return g_now; }
enum { P_RATE = 1 };
typedef union { uint32_t u32; } app_driver_param_val_t;
#define APP_DRIVER_SOURCE_LOCAL 0
static uint32_t g_rate = 999;
static esp_err_t app_driver_set_param(int, app_driver_param_val_t v, int) { g_rate = v.u32; return ESP_OK; }
${fill('config_defines.h')}
${fill('driver_statics.c')}
int main() {
    ${body}
    return 0;
}`
  const dir = mkdtempSync(join(tmpdir(), 'zc-pc-'))
  try {
    writeFileSync(join(dir, 'a.cpp'), prog)
    execFileSync(CC, ['-std=c++17', '-w', '-o', join(dir, 'a.out'), join(dir, 'a.cpp')], { stdio: 'pipe' })
    return execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim()
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
}

// 10 pulses per second, published every ~1000 ms with the timer late by 0.5 ms.
const steady = (pubs) => `
    int zeros = 0, last = 0;
    for (int p = 1; p <= ${pubs}; p++) {
        for (int k = 0; k < 10; k++) { g_now = (int64_t)(p - 1) * 1000500 + k * 100000 + 1; pc_pc_isr(nullptr); }
        g_now = (int64_t)p * 1000500;
        pc_pc_publish_cb(nullptr);
        if (p > 1 && g_rate == 0) zeros++;
        last = (int)g_rate;
    }`

test('pulse_counter: a window equal to the publish period never reads 0 on a steady input', { skip: !haveCc && 'no host C++ compiler' }, () => {
  assert.equal(run({}, steady(20) + `\n    printf("%d %d\\n", zeros, last >= 590 && last <= 610);`), '0 1')
})

test('pulse_counter: a stopped input still reads 0', { skip: !haveCc && 'no host C++ compiler' }, () => {
  assert.equal(run({}, steady(5) + `
    for (int p = 6; p <= 8; p++) { g_now = (int64_t)p * 1000500; pc_pc_publish_cb(nullptr); }
    printf("%u\\n", g_rate);`), '0')
})
