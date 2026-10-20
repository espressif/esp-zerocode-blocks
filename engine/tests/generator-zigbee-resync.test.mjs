// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// The Zigbee shell replays bus state to the hub after a reboot or a join. For a
// controller (a remote) every param is a COMMAND to bound devices, so the
// replay must be marked and the controller cases must skip it — otherwise a
// remote switches every bound light off each time it reboots. Generated over
// the real catalog so a framework or block edit that loses either half fails.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

function assembled() {
  const out = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(out, 'code_blocks'), '--products-out', join(out, 'product_configurations')], { stdio: 'ignore' })
  return out
}

test('zigbee resync never replays a controller command', async () => {
  const tpl = assembled()
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'zigbee-remote')
  try {
    const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
    const products = await eng.listAllProducts(paths)
    const entry = products.find((p) => p.product.id === 'zigbee-remote' || p.id === 'zigbee-remote')
    assert.ok(entry, 'zigbee-remote in the catalog')
    await eng.generate(paths, { product: entry.product, board: null, outDir, chip: 'esp32c6' })
    const src = readFileSync(join(outDir, 'components/app_zigbee/app_zigbee.cpp'), 'utf8')
    const resync = src.slice(src.indexOf('static void zigbee_resync_cb'))
    const body = resync.slice(0, resync.indexOf('\n}\n'))
    assert.match(body, /zigbee_driver_cb\([^;]*ZB_SOURCE_RESYNC, NULL\)/, 'resync passes its own source')
    assert.match(body, /if \(!app_driver_param_seen\(/, 'resync skips params never written')
    // every command a controller case sends is guarded against the replay
    const cb = src.slice(src.indexOf('static void zigbee_driver_cb'))
    const cbBody = cb.slice(0, cb.indexOf('\n}\n'))
    const guards = cbBody.match(/^\s*if \(param_id == .*\{$/gm) ?? []
    assert.ok(guards.length >= 2, 'controller cases rendered')
    for (const g of guards) assert.match(g, /source != ZB_SOURCE_RESYNC/, `guarded: ${g.trim()}`)
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true })
    rmSync(dirname(outDir), { recursive: true, force: true })
  }
})
