#!/usr/bin/env node
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Loader unit test — standalone, using the in-repo engine. No monorepo, no deps.
//   node scripts/test-loader.mjs
// Exercises the block read path (listAllBlocks / loadBlock), including the
// product-local `local/<name>` overlay that validate.mjs does not cover.
// Exits non-zero on the first failed assertion.
import { execFileSync } from 'node:child_process'
import { mkdtempSync, mkdirSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { dirname } from 'node:path'

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

let failures = 0
function check(name, cond) {
  if (cond) {
    console.log(`  ok   ${name}`)
  } else {
    console.error(`  FAIL ${name}`)
    failures++
  }
}

// Assemble the catalog to the single-file format listAllBlocks reads (same as
// validate.mjs), so the test runs against the real blocks.
const templatesDir = join(mkdtempSync(join(tmpdir(), 'zc-asm-')), 'templates')
execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
  '--out', join(templatesDir, 'code_blocks'),
  '--products-out', join(templatesDir, 'product_configurations')], { stdio: 'ignore' })

// A product-local blocks dir: one valid block + one entry that must be skipped.
const localBlocksDir = mkdtempSync(join(tmpdir(), 'zc-local-'))
mkdirSync(join(localBlocksDir, 'test_widget'))
writeFileSync(join(localBlocksDir, 'test_widget', 'block.yml'),
  'id: local/test_widget\nkind: device_type\ndescription: loader test fixture\nslots:\n  matter_includes.h: "// test"\n')
mkdirSync(join(localBlocksDir, 'not_a_block'))
writeFileSync(join(localBlocksDir, 'not_a_block', 'block.yml'), 'hello: world\n')

// Catalog reads (the pre-existing path).
const catalog = await eng.listAllBlocks({ templatesDir })
check('listAllBlocks loads the catalog', catalog.length > 0)
check('catalog has no local/ ids without a localBlocksDir',
  catalog.every((b) => !b.id.startsWith('local/')))

// Catalog + product-local overlay.
const withLocal = await eng.listAllBlocks({ templatesDir, localBlocksDir })
const widget = withLocal.find((b) => b.id === 'local/test_widget')
check('local/<name> resolves with its declared kind',
  widget !== undefined && widget.kind === 'device_type')
check('entry without a valid kind is skipped',
  withLocal.find((b) => b.id === 'local/not_a_block') === undefined)
check('overlay leaves the catalog blocks untouched',
  withLocal.length === catalog.length + 1)

// Single-block reads route through the local/ prefix too.
const loaded = await eng.loadBlock({ templatesDir, localBlocksDir }, 'local/test_widget')
check('loadBlock resolves a local/ ref', loaded.kind === 'device_type')

let thrownMsg = ''
try {
  await eng.loadBlock({ templatesDir }, 'local/test_widget')
} catch (e) {
  thrownMsg = e instanceof Error ? e.message : String(e)
}
// Assert the SPECIFIC error, not just that something threw — otherwise a
// file-not-found would satisfy this for the wrong reason.
check('loadBlock throws the "no localBlocksDir" error for a local/ ref',
  thrownMsg.includes('no localBlocksDir is configured'))

console.log(`\nLoader test — ${failures} failure(s)`)
if (failures) process.exit(1)
