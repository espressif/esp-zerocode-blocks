// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// A param of the product that no listener device type receives is still taken
// off the air by name, as an event with espnow's own handle (never re-sent);
// a framework-owned param is not.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('a product param sent by a peer lands on the bus as an event', async () => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  try {
    const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
    const product = {
      id: 'bell', name: 'bell', frameworks: ['espnow'],
      instances: [
        { block: 'drivers/button', prefix: 'BTN', cfg: { gpio: 9, target_param: 'APP_DRIVER_PARAM_BELL_ARMED', press_param: 'APP_DRIVER_PARAM_RING' } },
        { block: 'device_types/scene_button', prefix: 'BELL', cfg: { position_param: 'APP_DRIVER_PARAM_RING' } },
      ],
    }
    await eng.generate(paths, { product, board: null, outDir, chip: 'esp32c6' })
    const cpp = readFileSync(join(outDir, 'components/app_espnow/app_espnow.cpp'), 'utf8')
    assert.match(cpp, /\{ "APP_DRIVER_PARAM_RING", APP_DRIVER_PARAM_RING, 1 \}/)
    assert.match(cpp, /app_driver_fire_event\(s_now_params\[i\]\.id, v, s_handle\);/)
    const logic = readFileSync(join(outDir, 'components/app_logic/control.cpp'), 'utf8')
    assert.match(logic, /app_driver_fire_event\(APP_DRIVER_PARAM_RING,/, 'press_param fires on every press')
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true })
    rmSync(dirname(outDir), { recursive: true, force: true })
  }
})

test('a param the product never sends, such as a relay output, is not taken from the air; a product-declared one is', async () => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  try {
    const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
    const product = {
      id: 'bell', name: 'bell', frameworks: ['espnow'],
      params: { APP_DRIVER_PARAM_CHIME_MODE: 'u8' },
      instances: [
        { block: 'drivers/relay', prefix: 'DOOR', cfg: { gpio: 4, param_id: 'APP_DRIVER_PARAM_DOOR_STRIKE' } },
        { block: 'drivers/button', prefix: 'BTN', cfg: { gpio: 9, target_param: 'APP_DRIVER_PARAM_BELL_ARMED', press_param: 'APP_DRIVER_PARAM_RING' } },
        { block: 'device_types/scene_button', prefix: 'BELL', cfg: { position_param: 'APP_DRIVER_PARAM_RING' } },
      ],
    }
    await eng.generate(paths, { product, board: null, outDir, chip: 'esp32c6' })
    const cpp = readFileSync(join(outDir, 'components/app_espnow/app_espnow.cpp'), 'utf8')
    const table = cpp.slice(cpp.indexOf('s_now_params[] = {'), cpp.indexOf('};', cpp.indexOf('s_now_params[] = {')))
    assert.match(table, /"APP_DRIVER_PARAM_RING"/)
    assert.match(table, /"APP_DRIVER_PARAM_CHIME_MODE"/)
    assert.doesNotMatch(table, /DOOR_STRIKE/)
    assert.doesNotMatch(table, /BELL_ARMED/)
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true })
    rmSync(dirname(outDir), { recursive: true, force: true })
  }
})
