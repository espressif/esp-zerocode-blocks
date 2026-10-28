// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// The BLE HID framework is a media remote AND a keyboard: report id 2 carries
// keyboard keys, and a scene button with ble_hid_key sends one per press.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('a scene button sends a keyboard key through a keyboard report', async (t) => {
  const root = mkdtempSync(join(tmpdir(), 'zc-hid-kb-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const tpl = join(root, 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(root, 'out')
  const product = {
    id: 'clk', name: 'clk', frameworks: ['ble_hid'],
    instances: [
      { block: 'drivers/momentary_button', prefix: 'NEXTB', cfg: { gpio: 3, position_param: 'APP_DRIVER_PARAM_NEXT' } },
      { block: 'device_types/scene_button', prefix: 'NEXT', cfg: { position_param: 'APP_DRIVER_PARAM_NEXT', ble_hid_key: 79 } },
    ],
  }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c3' })
  const cpp = readFileSync(join(outDir, 'components/app_ble_hid/app_ble_hid.cpp'), 'utf8')
  assert.match(cpp, /0x09, 0x06,\s+\/\* Usage \(Keyboard\) \*\/[\s\S]*0x85, 0x02,/)
  assert.match(cpp, /void zc_ble_hid_send_key\(uint8_t keycode, uint8_t modifiers\)/)
  assert.match(cpp, /esp_hidd_dev_input_set\(s_hid_dev, 0, 2, report, 8\)/)
  assert.match(cpp, /if \(79 != 0\) \{\s*zc_ble_hid_send_key\(\(uint8_t\)79, 0\);/)
})
