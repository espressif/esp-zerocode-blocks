// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/console_param_cmd: a param held scaled (hundredths of a degree)
// takes the value as the owner types it, min/max refuse what the product does
// not allow (a wine cooler's target stored raw as 55 = 0.55 °F, unclamped),
// junk is refused, and a write a param guard refuses is reported as refused.
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/console_param_cmd/slots')

function run(cfg, body) {
  const full = { command_name: 'target', target_param: 'APP_DRIVER_PARAM_LEVEL', value_kind: 'u16', mode: 'state', scale: 1, min: 0, max: 0, ...cfg }
  const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'C', prefix_lc: 'c', cfg: full })
  const prog = `#include "app_driver.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <cmath>
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
static bool deny(app_driver_param_id_t, app_driver_param_val_t *v, app_driver_handle_t, void *) { return v->u16 != 7000; }
${fill('config_defines.h')}
${fill('logic_statics.c')}
static int cmd(const char *a) { char n[] = "target"; char x[32]; strcpy(x, a); char *argv[] = { n, x }; return c_cmd(2, argv); }
static unsigned lvl(void) { app_driver_param_val_t v = {}; app_driver_get_param(APP_DRIVER_PARAM_LEVEL, &v); return v.u16; }
int main()
{
    app_driver_set_guard(APP_DRIVER_PARAM_LEVEL, deny, nullptr);
    ${body}
    return 0;
}
`
  const dir = mkdtempSync(join(tmpdir(), 'zc-cpc-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), prog)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-pthread', '-DAPP_DRIVER_POLL_NO_TASK', '-I', join(BUS, 'test/shims'), '-I', join(BUS, 'test'), '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'main.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    return execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim().split('\n')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
}

test('console_param_cmd: scaled, ranged, junk refused, guard refusal reported', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const out = run({ scale: 100, min: 4100, max: 6500 }, `
    int a = cmd("55.5"); printf("rc=%d lvl=%u\\n", a, lvl());
    int b = cmd("70");   printf("rc=%d lvl=%u\\n", b, lvl());
    int c = cmd("55x");  printf("rc=%d lvl=%u\\n", c, lvl());`)
  assert.deepEqual(out, [
    'OK: target <- 55.5', 'rc=0 lvl=5550',
    'error: target must be 41.00 to 65.00', 'rc=1 lvl=5550',
    "error: '55x' is not a number", 'rc=1 lvl=5550',
  ])
})

test('console_param_cmd: defaults keep the raw type range; a guard refusal is not OK', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const out = run({}, `
    int a = cmd("300");   printf("rc=%d lvl=%u\\n", a, lvl());
    int b = cmd("70000"); printf("rc=%d lvl=%u\\n", b, lvl());
    int c = cmd("7000");  printf("rc=%d lvl=%u\\n", c, lvl());`)
  assert.deepEqual(out, [
    'OK: target <- 300', 'rc=0 lvl=300',
    'error: target must be 0 to 65535', 'rc=1 lvl=300',
    'refused: the product does not allow that now', 'rc=1 lvl=300',
  ])
})
