#!/usr/bin/env node
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Generate ONE product's firmware tree from this working tree, with the
// in-repo engine — no platform checkout, no network.
//
//   node scripts/generate.mjs <product-id> --chip <chip> [--out <dir>]
//
// Assembles the authoring tree (slots/, sidecars) into the consumption format,
// then runs the engine's generator. A product that ships a board.yaml beside
// its product.yml (the CI board samples) is generated for that board; its chip
// must agree with --chip. Board packs come from ZC_BOARDS_DIR, else the
// esp-board-manager checkout itself (BMGR_PATH — its esp_boards/ etc. ARE
// packs); with neither, a board product cannot be generated.
import { execFileSync } from 'node:child_process'
import { mkdtempSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

const args = process.argv.slice(2)
const id = args[0]
const flag = (name) => (args.includes(name) ? args[args.indexOf(name) + 1] : undefined)
const chip = flag('--chip')
if (!id || id.startsWith('--') || !chip) {
  console.error('usage: generate.mjs <product-id> --chip <chip> [--out <dir>]')
  process.exit(2)
}
const outDir = resolve(flag('--out') ?? join(ROOT, '_generated', id))

const asm = join(mkdtempSync(join(tmpdir(), 'zc-asm-')), 'templates')
execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
  '--out', join(asm, 'code_blocks'), '--products-out', join(asm, 'product_configurations')], { stdio: 'ignore' })

const boardsDir = process.env.ZC_BOARDS_DIR ?? process.env.BMGR_PATH
const paths = {
  templatesDir: asm,
  baseFirmwareDir: join(ROOT, 'base_firmware'),
  ...(boardsDir ? { boardsDir } : {}),
}
const product = await eng.loadProduct(paths, id)
// `board` omitted on purpose: the engine looks for the sample board.yaml beside
// the product — this loop has no user, so the catalog's fixture IS the board.
const result = await eng.generate(paths, { product, outDir, chip })
const board = await eng.loadBoardFile(paths, id)
if (board) console.log(`board: ${board.selected} → ${eng.bmgrBoardOf(board) ?? '(no definition — boardless tree)'}`)
console.log(`generated ${result.productId} for ${chip} → ${result.outDir} (${result.blocks.length} block instances)`)
