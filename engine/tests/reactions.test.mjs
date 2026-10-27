// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// A block that sends or drives something on a param change declares it
// (`reacts:`), and generate() returns those sentences with the params the
// slot dispatches on — what product code must not do a second time.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('reactsKeyOf maps dispatch slots to their family', () => {
  assert.equal(eng.reactsKeyOf('driver_apply_cases'), 'driver')
  assert.equal(eng.reactsKeyOf('ble_hid_driver_cb_cases'), 'ble_hid')
  assert.equal(eng.reactsKeyOf('zigbee_action_cases'), 'zigbee_action')
  assert.equal(eng.reactsKeyOf('main_init'), null)
})

test('collectReactions renders the sentence and keeps only consumed families', () => {
  const r = {
    block: { id: 'device_types/x', reacts: { ble_hid: 'Sends Mute on {{cfg.power_param}}.', espnow: 'Sends a peer command.' } },
    prefix: 'VOL',
    cfg: { power_param: 'APP_DRIVER_PARAM_POWER' },
    slots: {
      ble_hid_driver_cb_cases: 'case APP_DRIVER_PARAM_POWER: { send(); return; }\ncase APP_DRIVER_PARAM_DIAL_POSITION: { return; }',
      espnow_driver_cb_cases: 'case APP_DRIVER_PARAM_POWER: { return; }',
    },
  }
  const out = eng.collectReactions([r], ['ble_hid'])
  assert.deepEqual(out, [{ block: 'device_types/x', prefix: 'VOL', on: 'ble_hid', params: ['APP_DRIVER_PARAM_POWER', 'APP_DRIVER_PARAM_DIAL_POSITION'], does: 'Sends Mute on APP_DRIVER_PARAM_POWER.' }])
})

test('a composed BLE remote reports what it already sends', async () => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  try {
    const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
    const product = {
      id: 'knob', name: 'knob', frameworks: ['ble_hid'],
      instances: [{ block: 'device_types/dimmer_remote', prefix: 'VOL', cfg: { level_param: 'APP_DRIVER_PARAM_DIAL_POSITION' } }],
    }
    const res = await eng.generate(paths, { product, board: null, outDir, chip: 'esp32c3' })
    const hid = (res.reactions ?? []).find((x) => x.on === 'ble_hid')
    assert.ok(hid, 'the remote declares its ble_hid reaction')
    assert.deepEqual(hid.params.sort(), ['APP_DRIVER_PARAM_DIAL_POSITION', 'APP_DRIVER_PARAM_POWER'])
    assert.doesNotMatch(hid.does, /\{\{/)
    assert.ok(!(res.reactions ?? []).some((x) => x.on === 'espnow'), 'a framework the product did not select is not reported')
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true })
    rmSync(dirname(outDir), { recursive: true, force: true })
  }
})
