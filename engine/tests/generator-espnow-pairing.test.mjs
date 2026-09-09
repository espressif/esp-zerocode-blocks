// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// The ESP-NOW framework carries pairing: a persisted peer table, a window
// both sides open, unicast to stored peers, unknown senders dropped once
// paired. Generated over the real catalog so a framework edit that loses it
// fails here.
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

test('espnow framework generates the pairing protocol', async () => {
  const tpl = assembled()
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'espnow-light')
  try {
    const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
    const products = await eng.listAllProducts(paths)
    const entry = products.find((p) => p.product.id === 'espnow-light' || p.id === 'espnow-light')
    assert.ok(entry, 'espnow-light in the catalog')
    await eng.generate(paths, { product: entry.product, board: null, outDir, chip: 'esp32c3' })
    const src = readFileSync(join(outDir, 'components/app_espnow/app_espnow.cpp'), 'utf8')
    for (const sym of ['zc_espnow_pair_start', 'zc_now_peers_load', 'ZC_NOW_T_PAIR_REQ', 'ZC_NOW_T_PAIR_ACK',
                       'zc_now_peer_known(from)', 'espnow-pair', 'espnow-forget']) {
      assert.ok(src.includes(sym), `generated app_espnow.cpp carries ${sym}`)
    }
    // unicast to stored peers, broadcast only while none are stored
    assert.match(src, /if \(s_peer_count == 0\) \{\s*zc_now_send_raw\(ZC_NOW_BCAST/)
    const cmake = readFileSync(join(outDir, 'components/app_espnow/CMakeLists.txt'), 'utf8')
    assert.match(cmake, /esp_timer console/)
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true })
    rmSync(dirname(outDir), { recursive: true, force: true })
  }
})
