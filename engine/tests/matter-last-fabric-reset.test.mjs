// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Removing a Matter device from its last fabric factory-resets it through
// app_utils_factory_reset(), the reset every other path takes: the product's
// hook, then the whole NVS partition (the product's saved state included), and
// app_matter links app_utils for it
// on every Matter tree — the engine's CMakeLists replaces a catalog copy that
// lacks it, with or without device types.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('the last-fabric reset is the full factory reset, not Matter\'s own namespaces only', () => {
  const src = eng.genAppMatterCpp([])
  assert.match(src, /#include "app_utils\.h"/)
  assert.match(src, /FabricCount\(\) == 0\) \{\n[^\n]*\n\s+app_utils_factory_reset\(\);\n\s+\}/)
  assert.doesNotMatch(src, /esp_matter::factory_reset\(\)/)
})

test('a framework-only Matter tree links app_utils, even from a catalog whose CMakeLists does not', async () => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const f = join(tpl, 'code_blocks/frameworks/matter/components/app_matter/CMakeLists.txt')
  writeFileSync(f, readFileSync(f, 'utf8').replace(/ app_utils\b/, ''))
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  try {
    const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
    const entry = (await eng.listAllProducts(paths)).find((p) => (p.product?.id ?? p.id) === 'smart-plug')
    const product = { ...entry.product, instances: [{ block: 'behaviors/uptime_log', prefix: 'UP', cfg: { interval_seconds: 300 } }] }
    await eng.generate(paths, { product, board: null, outDir, chip: 'esp32c6' })
    const cmake = readFileSync(join(outDir, 'components/app_matter/CMakeLists.txt'), 'utf8')
    assert.match(cmake, /PRIV_REQUIRES[^\n]*\bapp_utils\b/)
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true })
    rmSync(dirname(outDir), { recursive: true, force: true })
  }
})
