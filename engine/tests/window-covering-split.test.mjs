// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// device_types/window_covering with current_param + stop_param: commands land
// on position_param (the target), the measured position is reported from
// current_param (null while unknown), and Stop is an event.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

async function generate(frameworks, split) {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'blind')
  const extra = split ? { current_param: 'APP_DRIVER_PARAM_LIFT_CURRENT', stop_param: 'APP_DRIVER_PARAM_LIFT_STOP' } : {}
  const product = {
    id: 'blind', name: 'blind', frameworks,
    exclude: ['behaviors/ota_basic', 'behaviors/network_status_log', 'behaviors/uptime_log', 'behaviors/wifi_disconnect_recovery'],
    instances: [
      { block: 'drivers/curtain_motor', prefix: 'MOTOR', cfg: { up_gpio: 4, down_gpio: 5, upper_limit_gpio: 6, lower_limit_gpio: 7, position_param: 'APP_DRIVER_PARAM_LIFT_POSITION', ...extra } },
      { block: 'device_types/window_covering', prefix: 'SHADE', cfg: { position_param: 'APP_DRIVER_PARAM_LIFT_POSITION', ...extra } },
    ],
  }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c6' })
  const read = (p) => readFileSync(join(outDir, p), 'utf8')
  return { read, cleanup: () => { rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true }) } }
}

test('split Matter covering: Current from current_param, Target mirror, Stop delegate', async () => {
  const g = await generate(['matter'], true)
  try {
    const src = g.read('components/app_matter/app_matter.cpp')
    assert.match(src, /#define SHADE_WC_SPLIT \(0 \+ 1\)/)
    assert.match(src, /if \(param_id == APP_DRIVER_PARAM_LIFT_CURRENT\)[\s\S]*?nullable<uint16_t>\(\)[\s\S]*?CurrentPositionLiftPercent100ths/)
    assert.match(src, /if \(param_id == APP_DRIVER_PARAM_LIFT_POSITION && val\.u16 <= 10000 && source != s_handle\)[\s\S]*?TargetPositionLiftPercent100ths/)
    assert.match(src, /HandleStopMotion\(\) override[\s\S]*?app_driver_fire_event\(APP_DRIVER_PARAM_LIFT_STOP/)
    assert.match(src, /window_covering\.delegate = &s_shade_wc_delegate/)
    const drv = g.read('components/app_driver/app_driver.cpp')
    assert.match(drv, /case APP_DRIVER_PARAM_LIFT_STOP:/)
    assert.match(drv, /app_driver_set_param\(APP_DRIVER_PARAM_LIFT_CURRENT/)
  } finally { g.cleanup() }
})

test('plain covering keeps one param: no delegate, no current param', async () => {
  const g = await generate(['matter'], false)
  try {
    const src = g.read('components/app_matter/app_matter.cpp')
    assert.match(src, /#define SHADE_WC_SPLIT \(0\)/)
    assert.doesNotMatch(src, /HandleStopMotion/)
    assert.doesNotMatch(src, /LIFT_CURRENT/)
  } finally { g.cleanup() }
})
