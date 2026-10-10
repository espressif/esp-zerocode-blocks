// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Default blocks: base_firmware/defaults.yml plus each framework's `defaults:`
// reach every generated tree, once, unless the product excludes them.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, existsSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parse as parseYaml } from 'yaml'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

const tmp = mkdtempSync(join(tmpdir(), 'zc-defaults-'))
const templatesDir = join(tmp, 'templates')
execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
  '--out', join(templatesDir, 'code_blocks'), '--products-out', join(templatesDir, 'product_configurations')], { stdio: 'ignore' })
const paths = { templatesDir, baseFirmwareDir: join(ROOT, 'base_firmware') }
process.on('exit', () => rmSync(tmp, { recursive: true, force: true }))

const BASE = parseYaml(readFileSync(join(ROOT, 'base_firmware/defaults.yml'), 'utf-8')).defaults
const MATTER = (await eng.loadBlock(paths, 'frameworks/matter')).defaults

const product = (over = {}) => ({
  id: 'fx', name: 'fx', description: 'fx', keywords: [], frameworks: [],
  instances: [{ block: 'drivers/status_led', prefix: 'LED', cfg: { gpio: 8, active_level: 1 } }],
  ...over,
})

async function gen(p, chip = 'esp32c6') {
  const outDir = join(tmp, `${p.id}-${chip}-${Math.random().toString(36).slice(2)}`)
  return { ...(await eng.generate(paths, { product: p, outDir, chip, board: null })), outDir }
}

const count = (blocks, id) => blocks.filter((b) => b === id).length

test('the base and Matter lists are non-empty data', () => {
  assert.ok(BASE.length > 0)
  assert.ok(MATTER.length > 0)
})

test('a product with no framework gets the base defaults', async () => {
  const { blocks, outDir } = await gen(product())
  for (const id of BASE) assert.equal(count(blocks, id), 1, id)
  for (const id of MATTER) assert.equal(count(blocks, id), 0, id)
  assert.ok(!existsSync(join(outDir, 'defaults.yml')), 'the list itself is not copied into the tree')
})

test('a Matter product also gets the Matter defaults', async () => {
  const { blocks } = await gen(product({ frameworks: ['matter'] }))
  for (const id of [...BASE, ...MATTER]) assert.equal(count(blocks, id), 1, id)
})

test('a default the product already lists is not added again', async () => {
  const p = product({
    frameworks: ['matter'],
    instances: [...product().instances, { block: 'behaviors/ota_basic', prefix: 'OTA', cfg: {} }],
  })
  const { blocks } = await gen(p)
  assert.equal(count(blocks, 'behaviors/ota_basic'), 1)
})

test('exclude removes a base and a framework default', async () => {
  const p = product({ frameworks: ['matter'], exclude: ['behaviors/boot_count', 'behaviors/ota_basic'] })
  const { blocks } = await gen(p)
  assert.equal(count(blocks, 'behaviors/boot_count'), 0)
  assert.equal(count(blocks, 'behaviors/ota_basic'), 0)
  assert.equal(count(blocks, 'behaviors/task_watchdog'), 1)
})

test('an early-build composition (frameworks plus one neutral instance) includes the defaults', async () => {
  const early = product({
    id: 'early-build',
    frameworks: ['matter'],
    instances: [{ block: 'behaviors/uptime_log', prefix: 'EARLY_UPTIME', cfg: {} }],
  })
  const { blocks } = await gen(early, 'esp32c3')
  for (const id of [...BASE, ...MATTER]) assert.equal(count(blocks, id), 1, id)
})

test('the validator applies the defaults and warns on an exclude that is not one', async () => {
  const all = await eng.listAllBlocks(paths)
  const map = new Map(all.map((b) => [b.id, b.block]))
  const r = eng.validateProduct(
    product({ frameworks: ['matter'], exclude: ['behaviors/ota_basic', 'drivers/relay'] }),
    map,
    { baseDefaults: BASE },
  )
  assert.deepEqual(r.errors, [])
  assert.ok(r.warnings.some((w) => /exclude: 'drivers\/relay'/.test(w.message)))
  assert.ok(!r.warnings.some((w) => /exclude: 'behaviors\/ota_basic'/.test(w.message)))
})

test('validateDefaults refuses a missing block, a non-behavior and a chip-only block', async () => {
  const all = await eng.listAllBlocks(paths)
  const map = new Map(all.map((b) => [b.id, b.block]))
  assert.deepEqual(eng.validateDefaults([...BASE, ...MATTER], map, 'x'), [])
  const problems = eng.validateDefaults(['behaviors/nope', 'drivers/status_led', 'sdkconfig-fragments/esp32h2'], map, 'x')
  assert.equal(problems.length, 3)
})
