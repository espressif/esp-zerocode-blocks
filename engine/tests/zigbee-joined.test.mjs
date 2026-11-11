// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// frameworks/zigbee publishes whether the device is on a network as
// APP_DRIVER_PARAM_ZIGBEE_JOINED: true on a join or a reboot onto the network,
// false while factory-new and after a leave.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('the joined state follows the stack signals', async (t) => {
  const root = mkdtempSync(join(tmpdir(), 'zc-zb-joined-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const tpl = join(root, 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(root, 'out')
  const product = { id: 'z', name: 'z', frameworks: ['zigbee'], instances: [
    { block: 'drivers/pwm_led', prefix: 'LIGHT', cfg: { gpio: 4 } },
    { block: 'device_types/dimmable_light', prefix: 'LAMP', cfg: {} },
  ] }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c6' })
  const cpp = readFileSync(join(outDir, 'components/app_zigbee/app_zigbee.cpp'), 'utf8')
  assert.match(cpp, /static void zc_zigbee_publish_joined\(bool on\)/)
  assert.match(cpp, /"Joined network successfully"\);\s*zc_zigbee_publish_joined\(true\);/)
  assert.match(cpp, /"Device rebooted on network"\);\s*zc_zigbee_publish_joined\(true\);/)
  assert.match(cpp, /open pairing on your Zigbee hub"\);\s*zc_zigbee_publish_joined\(false\);/)
  assert.match(cpp, /steering again; open pairing on your hub"\);\s*zc_zigbee_publish_joined\(false\);/)
  const types = readFileSync(join(outDir, 'components/app_driver/include/app_driver_types.h'), 'utf8')
  assert.match(types, /APP_DRIVER_PARAM_ZIGBEE_JOINED,\s*\/\* bool/)
})
