#!/usr/bin/env node
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Full semantic validation — standalone, using the in-repo engine. No monorepo.
//   node scripts/validate.mjs [--strict] [--product <id>]
// Assembles the working tree, then runs the engine's validator over it.
import { execFileSync } from 'node:child_process'
import { mkdtempSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { dirname } from 'node:path'

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

const args = process.argv.slice(2)
const strict = args.includes('--strict')
const product = args.includes('--product') ? args[args.indexOf('--product') + 1] : null

const out = join(mkdtempSync(join(tmpdir(), 'zc-asm-')), 'templates')
execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
  '--out', join(out, 'code_blocks'), '--products-out', join(out, 'product_configurations')], { stdio: 'ignore' })

const paths = { templatesDir: out }
const blocks = await eng.listAllBlocks(paths)
const blockMap = new Map(blocks.map((b) => [b.id, b.block]))
const products = await eng.listAllProducts(paths)

const errors = []
const warnings = []
for (const { id, block } of blocks) {
  const r = eng.validateBlock({ ...block, id: block.id ?? id })
  if (block.id && block.id !== id) errors.push({ block: id, message: `id mismatch: file says '${block.id}', dir implies '${id}'` })
  errors.push(...r.errors); warnings.push(...r.warnings)
}
for (const { id, product: p } of products) {
  if (product && p.id !== product) continue
  // The board is a SEPARATE document (board.yaml — see engine/src/types.ts):
  // the product says what it does, the board says what hardware one instance
  // of it runs on. Only the two board sample products carry one in the
  // catalog; for every other product this is null and no board check runs.
  let board = null
  try {
    board = await eng.loadBoardFile(paths, id)
  } catch (e) {
    errors.push({ product: p.id, message: `board.yaml: ${e.message}` })
  }
  const r = eng.validateProduct(p, blockMap, { board })
  errors.push(...r.errors); warnings.push(...r.warnings)
}

const show = (lvl, i) => `${lvl} ${i.product ? `[product ${i.product}]` : i.block ? `[block ${i.block}]` : ''} ${i.message}`
for (const w of warnings) console.warn(show('WARN ', w))
for (const e of errors) console.error(show('ERROR', e))
console.log(`\nValidated: ${blocks.length} blocks, ${products.length} products — ${errors.length} errors, ${warnings.length} warnings`)
if (errors.length || (strict && warnings.length)) process.exit(1)
