// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/pulse_counter on the host: a 100 Hz tachometer at 2 pulses per
// revolution reads 3000 RPM, a stopped input reads 0 after the window, and
// edges closer than min_interval_us count once.
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
const SLOTS = join(ROOT, 'code_blocks/drivers/pulse_counter/slots')
const cfg = { gpio: 7, edge: 'falling', pull: 'up', min_interval_us: 100, pulses_per_unit: 2, rate_window_ms: 2000, publish_ms: 1000, count_param: 'P_COUNT', rate_param: 'P_RATE' }
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'TACH', prefix_lc: 'tach', cfg })

const HARNESS = (defines, statics) => String.raw`
#include <cstdint>
#include <cstdio>
#include <string>
#include "esp_timer.h"
#include "freertos/portmacro.h"
#define IRAM_ATTR
#define portENTER_CRITICAL_ISR(m) portENTER_CRITICAL(m)
#define portEXIT_CRITICAL_ISR(m) portEXIT_CRITICAL(m)
enum { P_COUNT, P_RATE };
union app_driver_param_val_t { bool b; uint8_t u8; uint16_t u16; uint32_t u32; };
#define APP_DRIVER_SOURCE_LOCAL 0
static uint32_t g_val[2];
static int app_driver_set_param(int id, app_driver_param_val_t v, int) { g_val[id] = v.u32; return 0; }
${defines}
${statics}
static std::string out;
static void tick_publish(void) { tach_pc_publish_cb(nullptr); out += std::to_string(g_val[P_COUNT]) + "/" + std::to_string(g_val[P_RATE]) + " "; }
int main()
{
    zc_test_now_us = 1000000;
    for (int s = 0; s < 3; s++) {                 /* 100 Hz for 3 s */
        for (int i = 0; i < 100; i++) { zc_test_now_us += 10000; tach_pc_isr(nullptr); }
        tick_publish();
    }
    for (int s = 0; s < 3; s++) { zc_test_now_us += 1000000; tick_publish(); }   /* stopped */
    zc_test_now_us += 1000; tach_pc_isr(nullptr); zc_test_now_us += 50; tach_pc_isr(nullptr);  /* a glitch */
    tick_publish();
    printf("%s\n", out.c_str());
    return 0;
}
`

test('pulse_counter: rate in units per minute, 0 when stopped, glitches filtered', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-pc-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), HARNESS(fill('config_defines.h'), fill('driver_statics.c')))
    execFileSync(CXX, ['-std=c++20', '-Wno-unused-function', '-pthread', '-I', join(BUS, 'test/shims'), '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    const got = execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim().split(' ')
    assert.equal(got[0].split('/')[0], '100', 'first second: 100 pulses')
    assert.equal(got[2], '300/3000', '100 Hz at 2 pulses per revolution is 3000 RPM')
    assert.equal(got[4], '300/0', 'stopped: 0 once the window passes')
    assert.equal(got[6].split('/')[0], '301', 'two edges 50 us apart count once')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
