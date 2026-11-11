// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// A product's own params (template.yaml `params:`) join the param enum: a
// product with state no block declares used to redefine APP_DRIVER_PARAM_MAX
// by hand, and its own #error stopped every build of the fix.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

async function gen(params) {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  const product = { id: 'p', name: 'p', frameworks: [], params, instances: [
    { block: 'drivers/relay', prefix: 'APP', cfg: { gpio: 4, active_level: 1, param_id: 'APP_DRIVER_PARAM_POWER' } }] }
  try {
    await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c3' })
    return readFileSync(join(outDir, 'components/app_driver/include/app_driver_types.h'), 'utf8')
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true })
  }
}

test('product params join the param enum with their type', async () => {
  const h = await gen({ APP_DRIVER_PARAM_SETPOINT_F: 'i16', APP_DRIVER_PARAM_COOLING_TODAY: 'u32' })
  assert.match(h, /APP_DRIVER_PARAM_SETPOINT_F,\s+\/\*.*\.i16/)
  assert.match(h, /APP_DRIVER_PARAM_COOLING_TODAY,\s+\/\*.*\.u32/)
  assert.match(h, /APP_DRIVER_PARAM_POWER,/)
})

test('product params: a bad name, a bad type, or a type a block contradicts is refused', async () => {
  await assert.rejects(() => gen({ setpoint: 'i16' }), /is not a param name/)
  await assert.rejects(() => gen({ APP_DRIVER_PARAM_X: 'float' }), /has type 'float'/)
  await assert.rejects(() => gen({ APP_DRIVER_PARAM_POWER: 'u8' }), /the product declares u8, a block declares it bool/)
})
