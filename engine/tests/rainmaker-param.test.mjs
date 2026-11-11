// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/rainmaker_param puts a product's own param in the RainMaker app as
// its own tile, typed by its kind, written through the param bus and never
// persisted by RainMaker itself.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('a product setting is a RainMaker tile on the param bus, never persisted by RainMaker', async (t) => {
  const root = mkdtempSync(join(tmpdir(), 'zc-rmp-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const tpl = join(root, 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const product = {
    id: 'aq', name: 'aq', frameworks: ['rainmaker'],
    instances: [
      { block: 'drivers/pwm_led', prefix: 'LED', cfg: { gpio: 4 } },
      { block: 'device_types/dimmable_light', prefix: 'TANK', cfg: { label: 'Tank light' } },
      { block: 'behaviors/rainmaker_param', prefix: 'ON_AT', cfg: { label: 'Lights on at', kind: 'time', param: 'APP_DRIVER_PARAM_ON_AT', value_kind: 'u16' } },
    ],
  }
  const blocks = new Map((await eng.listAllBlocks(paths)).map((b) => [b.id, b.block]))
  assert.deepEqual(eng.validateProduct(product, blocks).errors.map((e) => e.message), [])
  const outDir = join(root, 'out')
  const res = await eng.generate(paths, { product, board: null, outDir, chip: 'esp32c3' })
  const cpp = readFileSync(join(outDir, 'components/app_rainmaker/app_rainmaker.cpp'), 'utf8')
  assert.match(cpp, /esp_rmaker_param_create\("Lights on at", "esp\.param\.zc-setting"/)
  assert.doesNotMatch(cpp.slice(cpp.indexOf('s_on_at_rmp_param = esp_rmaker_param_create'), cpp.indexOf('s_on_at_rmp_param = esp_rmaker_param_create') + 400), /PROP_FLAG_PERSIST/)
  assert.match(cpp, /app_driver_set_param\(APP_DRIVER_PARAM_ON_AT, pv, s_handle\)/)
  const cfg = readFileSync(join(outDir, 'components/app_config/include/app_config.h'), 'utf8')
  assert.match(cfg, /#define ON_AT_RMP_KIND_time\s+1/)
  assert.ok((res.reactions ?? []).some((r) => r.block === 'behaviors/rainmaker_param' && /Lights on at/.test(r.does)))
})
