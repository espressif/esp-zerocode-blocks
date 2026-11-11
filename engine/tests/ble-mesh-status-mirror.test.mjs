// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// A mesh dimmer remote mirrors the lights' reported OnOff and Level Status
// onto its own params (level on the 0-254 scale), written as the mesh
// framework so it is never published back.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('the lights\' reported state lands on the remote\'s params', async (t) => {
  const root = mkdtempSync(join(tmpdir(), 'zc-mesh-mirror-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const tpl = join(root, 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(root, 'out')
  const product = { id: 'sw', name: 'sw', frameworks: ['ble_mesh'], instances: [{ block: 'device_types/dimmer_remote', prefix: 'SW', cfg: {} }] }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c6' })
  const app = readFileSync(join(outDir, 'components/app_ble_mesh/app_ble_mesh.cpp'), 'utf8')
  assert.match(app, /sw_st\.b = on;\s*app_driver_set_param\(APP_DRIVER_PARAM_POWER, sw_st, s_handle\);[\s\S]{0,200}zc_ble_mesh_on_onoff_status\(src, on\)/)
  assert.match(app, /sw_st\.u8 = zc_ble_mesh_level_to_u8\(level\);\s*app_driver_set_param\(APP_DRIVER_PARAM_BRIGHTNESS, sw_st, s_handle\);[\s\S]{0,200}zc_ble_mesh_on_level_status\(src, level\)/)
})
