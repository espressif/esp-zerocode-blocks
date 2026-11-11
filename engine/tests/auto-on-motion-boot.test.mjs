// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/auto_on_motion on the REAL param bus: motion reported before the
// block registered (a sensor that read "occupied" at boot) still switches the
// light on; a later edge still does.
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/auto_on_motion/slots')
const cfg = { motion_param: 'P_MOTION', light_param: 'P_LIGHT' }
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'AUTO', prefix_lc: 'auto', cfg })

const HARNESS = (statics, init) => String.raw`
#include "app_driver.h"
#include <cstdio>
#define ESP_LOGI(tag, ...) ((void)(tag))
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
static const char *TAG = "t";
#define P_MOTION ((app_driver_param_id_t)1)
#define P_LIGHT ((app_driver_param_id_t)2)
${statics}
static bool get(app_driver_param_id_t id) { app_driver_param_val_t v = {}; app_driver_get_param(id, &v); return v.b; }
static void set(app_driver_param_id_t id, bool b) { app_driver_param_val_t v = {}; v.b = b; app_driver_set_param(id, v, APP_DRIVER_SOURCE_LOCAL); }
int main()
{
    set(P_MOTION, true);                 /* the sensor reads "occupied" first */
    ${init}
    int at_boot = get(P_LIGHT);
    set(P_LIGHT, false); set(P_MOTION, false); set(P_MOTION, true);
    printf("%d %d\n", at_boot, (int)get(P_LIGHT));
    return 0;
}
`

test('auto_on_motion: motion present before it registered still switches the light on', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-aom-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), HARNESS(fill('logic_statics.c'), fill('logic_init.c')))
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-pthread', '-I', join(BUS, 'test/shims'), '-I', join(BUS, 'test'), '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'main.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '1 1')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
