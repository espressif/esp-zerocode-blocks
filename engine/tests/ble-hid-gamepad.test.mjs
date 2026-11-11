// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// frameworks/ble_hid + device_types/gamepad: composing the gamepad adds one
// well-formed gamepad report (id 3, 16 buttons + 4 axes = 6 bytes) with the
// gamepad appearance and the block's name; without it the media remote's
// report map, name and appearance are unchanged.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

async function generated(instances) {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  const product = { id: 'p', name: 'p', frameworks: ['ble_hid'], instances }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c3' })
  const src = readFileSync(join(outDir, 'components/app_ble_hid/app_ble_hid.cpp'), 'utf8')
  rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true })
  return src
}

// A HID report descriptor walked item by item: collections must close, and
// each report id's INPUT bits are summed (report size × count per Input item).
function inputBits(src) {
  const body = src.slice(src.indexOf('s_report_map[] = {'), src.indexOf('};', src.indexOf('s_report_map[] = {')))
  const bytes = [...body.replace(/\/\*[\s\S]*?\*\//g, '').matchAll(/0x([0-9A-Fa-f]{2})/g)].map((m) => parseInt(m[1], 16))
  let i = 0, depth = 0, id = 0, size = 0, count = 0
  const bits = {}
  const usages = []
  while (i < bytes.length) {
    const p = bytes[i]; const n = [0, 1, 2, 4][p & 3]; const type = (p >> 2) & 3; const tag = p >> 4
    const data = bytes.slice(i + 1, i + 1 + n).reduce((a, b, k) => a | (b << (8 * k)), 0)
    assert.ok(i + 1 + n <= bytes.length, 'item runs past the end')
    if (type === 0 && tag === 0xA) depth++                       // Collection
    if (type === 0 && tag === 0xC) depth--                       // End Collection
    if (type === 0 && tag === 0x8) bits[id] = (bits[id] ?? 0) + size * count   // Input
    if (type === 1 && tag === 0x8) id = data                     // Report ID
    if (type === 1 && tag === 0x7) size = data                   // Report Size
    if (type === 1 && tag === 0x9) count = data                  // Report Count
    if (type === 2 && tag === 0x0 && depth === 0) usages.push(data)   // top-level Usage
    i += 1 + n
  }
  assert.equal(depth, 0, 'every collection closes')
  return { bits, usages }
}

test('ble_hid: a composed gamepad adds report 3 (6 bytes), its appearance and its name', async () => {
  const src = await generated([{ block: 'device_types/gamepad', prefix: 'PAD', cfg: { device_name: 'Couch Pad' } }])
  const { bits, usages } = inputBits(src)
  assert.deepEqual(bits, { 1: 16, 2: 64, 3: 48 })
  assert.deepEqual(usages, [0x01, 0x06, 0x05], 'consumer control, keyboard, game pad')
  assert.match(src, /void zc_ble_hid_send_gamepad\(uint16_t buttons, int8_t x, int8_t y, int8_t z, int8_t rz\)/)
  assert.match(src, /uint8_t report\[6\]/)
  assert.match(src, /ble_hid_gap_adv_init\(0x03C4 \/\* gamepad \*\/, "Couch Pad"\)/)
  assert.match(src, /hid_config\.device_name = "Couch Pad";/)
})

test('ble_hid: without the gamepad the media remote is unchanged', async () => {
  const src = await generated([{ block: 'behaviors/uptime_log', prefix: 'UP', cfg: {} }])
  assert.deepEqual(inputBits(src).bits, { 1: 16, 2: 64 })
  assert.doesNotMatch(src, /zc_ble_hid_send_gamepad|Game Pad/)
  assert.match(src, /ble_hid_gap_adv_init\(0x03C0 \/\* generic HID \*\/, "ZeroCode Remote"\)/)
  assert.match(src, /hid_config\.device_name = "ZeroCode Remote";/)
})
