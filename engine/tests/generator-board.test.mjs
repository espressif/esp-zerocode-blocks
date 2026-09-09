#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Unit tests for the generator's BOARD behaviour (esp-board-manager). The
 * board is a separate input — a parsed board.yaml — never a product key:
 *
 *   - the board's init lands BEFORE app_driver_init(), not after it;
 *   - a product with no board gets a byte-identical app_main.cpp, so adding
 *     the slot cost nothing to the other 74 products;
 *   - a board file whose bmgr.board is null (hardware we have no definition
 *     for yet) generates the SAME tree as no board at all;
 *   - the board owns memory facts: a partition table that does not fit the
 *     board's flash fails generation, and the product's contradicting
 *     flash-size keys are dropped rather than left to a load-order tiebreak;
 *   - a board file beside a catalog product.yml is found when the caller
 *     passes no board, which is what keeps the two board SAMPLES building.
 *
 * Standalone, like scripts/validate.mjs and validator-board.test.mjs: node's
 * built-in runner against engine/dist, with a base_firmware + board pack built
 * in a temp dir, so nothing here needs a board-pack checkout.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, rmSync, existsSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const { generate, partitionTableEnd, loadBoardFile, parseBoardYaml } =
  await import(join(ROOT, 'engine/dist/index.js'))

/* ── fixtures ────────────────────────────────────────────────────────── */

const APP_MAIN = `#include "app_driver.h"
{{main_includes}}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

{{board_init}}
    /* Initialize hardware drivers */
    ESP_ERROR_CHECK(app_driver_init());

    /* Initialize protocol solutions */
{{main_init}}
}
`

/** A 4 MB table (ends at 0x400000) and an 8 MB one (ends at 0x800000). */
const TABLE_4MB = 'nvs, data, nvs, 0x9000, 0x6000,\nfactory, app, factory, 0x10000, 0x3F0000,\n'
const TABLE_8MB = 'nvs, data, nvs, 0x9000, 0x6000,\nfactory, app, factory, 0x10000, 0x7F0000,\n'

function scaffold(t) {
  const root = mkdtempSync(join(tmpdir(), 'zc-gen-board-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))

  const base = join(root, 'base_firmware')
  mkdirSync(join(base, 'main'), { recursive: true })
  writeFileSync(join(base, 'main/app_main.cpp'), APP_MAIN)
  writeFileSync(join(base, 'main/idf_component.yml'), 'dependencies: {}\n')
  writeFileSync(join(base, 'partitions.csv'), TABLE_4MB)
  writeFileSync(join(base, 'sdkconfig.defaults'), '# base\n')

  const templates = join(root, 'templates')
  mkdirSync(join(templates, 'code_blocks'), { recursive: true })
  mkdirSync(join(templates, 'product_configurations'), { recursive: true })
  for (const [name, csv] of [['t4mb', TABLE_4MB], ['t8mb', TABLE_8MB]]) {
    const dir = join(templates, 'code_blocks/partition-tables', name)
    mkdirSync(dir, { recursive: true })
    writeFileSync(join(dir, 'partitions.csv'), csv)
    writeFileSync(join(dir, 'block.yml'), [
      'description: test table',
      'sdkconfig:',
      `  CONFIG_ESPTOOLPY_FLASHSIZE_${name === 't4mb' ? '4' : '8'}MB: "y"`,
      '',
    ].join('\n'))
  }

  // A board pack in the esp-board-manager shape: <root>/<pack>/<board>/.
  const boards = join(root, 'boards')
  const boardDir = join(boards, 'pack_one', 'demo_board')
  mkdirSync(boardDir, { recursive: true })
  writeFileSync(join(boardDir, 'board_info.yaml'), 'board: demo_board\nchip: esp32c3\n')
  writeFileSync(join(boardDir, 'sdkconfig.defaults.board'),
    '# demo board: 4 MB flash\nCONFIG_ESPTOOLPY_FLASHSIZE_4MB=y\nCONFIG_SPIRAM=n\n')

  return {
    paths: { templatesDir: templates, baseFirmwareDir: base, boardsDir: boards },
    outDir: join(root, 'out'),
  }
}

const product = (over = {}) => ({
  id: 'demo', name: 'demo', description: 'demo',
  frameworks: [], instances: [], ...over,
})

/** A board file — the hardware one INSTANCE of the product runs on. The board
 *  is not a product key (see engine/src/types.ts): it is a separate document
 *  handed to generate() beside the product. */
const boardFile = (bmgrBoard, over = {}) => ({
  schema: 'zc-board/1',
  selected: 'demo-devkit',
  chip: 'esp32c3',
  bmgr: bmgrBoard === null ? null : { board: bmgrBoard, resolved_from: 'exact' },
  ...over,
})

/* ── task A: board init precedes driver init ─────────────────────────── */

test('the board brings itself up BEFORE app_driver_init()', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, { product: product(), board: boardFile('demo_board'), chip: 'esp32c3', outDir })
  const main = readFileSync(join(outDir, 'main/app_main.cpp'), 'utf-8')

  const boardAt = main.indexOf('esp_board_manager_init()')
  const driverAt = main.indexOf('app_driver_init()')
  assert.ok(boardAt > 0, 'the board init call is rendered')
  assert.ok(boardAt < driverAt, 'the board comes up before the drivers that may use it')
  // The include stays where every other include is.
  assert.ok(main.includes('#include "esp_board_manager.h"'))
  // …and it is NOT in the protocol slot any more, which runs after the drivers.
  assert.ok(main.indexOf('esp_board_manager_init()') < main.indexOf('Initialize protocol solutions'))
})

test('a product with no board gets app_main.cpp with the slot LINES removed', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, { product: product(), board: null, chip: 'esp32c3', outDir })
  const main = readFileSync(join(outDir, 'main/app_main.cpp'), 'utf-8')

  assert.ok(!main.includes('{{'), 'no marker survives')
  assert.ok(!main.includes('esp_board_manager'), 'nothing board-shaped leaks in')
  // The whole LINE goes, not just the marker: a boardless tree must be
  // byte-identical to one generated before {{board_init}} existed.
  assert.ok(
    main.includes('    ESP_ERROR_CHECK(nvs_flash_init());\n\n    /* Initialize hardware drivers */'),
    'no blank line is left where the marker was',
  )
})

/* ── task B: the board owns memory facts ─────────────────────────────── */

test('a partition table larger than the board\'s flash fails generation', async (t) => {
  const { paths, outDir } = scaffold(t)
  await assert.rejects(
    generate(paths, { product: product({ partition_table: 't8mb' }), board: boardFile('demo_board'), chip: 'esp32c3', outDir }),
    (e) => {
      assert.match(e.message, /partition_table 't8mb'/)
      assert.match(e.message, /8 MB of flash/)
      assert.match(e.message, /board 'demo_board' has 4 MB/)
      assert.match(e.message, /CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y/)
      return true
    },
  )
})

test('a partition table that fits the board generates', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, { product: product({ partition_table: 't4mb' }), board: boardFile('demo_board'), chip: 'esp32c3', outDir })
  assert.ok(readFileSync(join(outDir, 'partitions.csv'), 'utf-8').includes('factory'))
})

test('the product does not restate a flash size the board already decides', async (t) => {
  const { paths, outDir } = scaffold(t)

  await generate(paths, { product: product({ partition_table: 't4mb' }), board: boardFile('demo_board'), chip: 'esp32c3', outDir })
  const withBoard = readFileSync(join(outDir, 'sdkconfig.defaults'), 'utf-8')
  assert.ok(!withBoard.includes('CONFIG_ESPTOOLPY_FLASHSIZE'), 'the board is the only voice on flash size')

  // Same table, no board: the product still carries its own flash size, which
  // is what makes the drop above a board rule rather than a table change.
  await generate(paths, { product: product({ partition_table: 't4mb' }), board: null, chip: 'esp32c3', outDir })
  const noBoard = readFileSync(join(outDir, 'sdkconfig.defaults'), 'utf-8')
  assert.ok(noBoard.includes('CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y'))
})

test('an unreachable board pack skips the gates instead of failing', async (t) => {
  const { paths, outDir } = scaffold(t)
  // A board name no pack defines: the engine cannot see its flash size, so it
  // must generate exactly as before rather than refuse.
  await generate(paths, { product: product({ partition_table: 't8mb' }), board: boardFile('not_in_any_pack'), chip: 'esp32c3', outDir })
  const main = readFileSync(join(outDir, 'main/app_main.cpp'), 'utf-8')
  assert.ok(main.includes('esp_board_manager_init()'))
})

/* ── the size arithmetic the gate rests on ───────────────────────────── */

test('partitionTableEnd walks blank offsets the way gen_esp32part.py does', () => {
  // Explicit offsets: the last byte of the last partition.
  assert.equal(partitionTableEnd(TABLE_4MB), 0x400000)
  // Blank offsets follow the previous partition, aligned (4 KB data / 64 KB app).
  const blanks = [
    'nvs,      data, nvs,      0x9000, 0x4000,',
    'otadata,  data, ota,      ,       0x2000,',
    'phy_init, data, phy,      ,       0x1000,',
    'factory,  app,  factory,  ,       0x10000,',
  ].join('\n')
  // nvs ends 0xD000, otadata 0xD000-0xF000, phy 0xF000-0x10000, app aligns to
  // 0x10000 and runs to 0x20000.
  assert.equal(partitionTableEnd(blanks), 0x20000)
  // Comments and K/M suffixes are ordinary CSV, not a parse failure.
  assert.equal(partitionTableEnd('# a comment\nfactory, app, factory, 0, 1M,\n'), 1024 * 1024)
})

/* ── the board is a property of the INSTANCE, not of the catalog ──────── */

test('a board file with bmgr.board null generates exactly the boardless tree', async (t) => {
  const { paths, outDir } = scaffold(t)
  // The user picked a bare module we have no bmgr definition for. That is a
  // coverage fact about our packs, not an error about their hardware — so it
  // degrades to the chip-agnostic path rather than failing or half-wiring one.
  await generate(paths, {
    product: product({ partition_table: 't4mb' }),
    board: { schema: 'zc-board/1', selected: 'esp32-c6-mini-1', chip: 'esp32c6', bmgr: { board: null, resolved_from: 'fallback' } },
    chip: 'esp32c6',
    outDir,
  })
  const main = readFileSync(join(outDir, 'main/app_main.cpp'), 'utf-8')
  assert.ok(!main.includes('esp_board_manager'), 'no board component is wired in')
  assert.equal(existsSync(join(outDir, '.zc-board')), false, 'and nothing tells the build step to run bmgr')
  // The product keeps its own flash-size key: with no definition there are no
  // board facts to outrank it.
  assert.ok(readFileSync(join(outDir, 'sdkconfig.defaults'), 'utf-8').includes('CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y'))
})

test('.zc-board carries the RESOLVED bmgr board, which is what the build step reads', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, {
    product: product(),
    // A fallback resolution: the user's hardware is a module, and what we can
    // generate against is the minimal board for its chip. The build must be
    // told the latter — `selected` is not a bmgr name and never resolves.
    board: { selected: 'demo-module', chip: 'esp32c3', bmgr: { board: 'demo_board', resolved_from: 'fallback' } },
    chip: 'esp32c3',
    outDir,
  })
  assert.equal(readFileSync(join(outDir, '.zc-board'), 'utf-8'), 'demo_board\n')
})

/* ── the sample board file beside a catalog product ──────────────────── */

test('a board.yaml beside product.yml is used when the caller passes no board', async (t) => {
  const { paths, outDir } = scaffold(t)
  const dir = join(paths.templatesDir, 'product_configurations', 'demo')
  mkdirSync(dir, { recursive: true })
  writeFileSync(join(dir, 'board.yaml'),
    'schema: zc-board/1\nselected: demo-devkit\nchip: esp32c3\nbmgr:\n  board: demo_board\n  resolved_from: exact\n')

  await generate(paths, { product: product(), chip: 'esp32c3', outDir })
  assert.equal(readFileSync(join(outDir, '.zc-board'), 'utf-8'), 'demo_board\n')

  // …and an explicit null says "no board", which must beat the lookup: a host
  // that owns the answer cannot have a catalog fixture put a board back.
  await generate(paths, { product: product(), board: null, chip: 'esp32c3', outDir })
  assert.equal(existsSync(join(outDir, '.zc-board')), false)
})

test('loadBoardFile returns null for a product with no board file', async (t) => {
  const { paths } = scaffold(t)
  assert.equal(await loadBoardFile(paths, 'demo'), null)
})

test('a board file missing selected/chip is refused, not half-honoured', () => {
  // The failure this prevents: a typo'd board file is ignored, the tree
  // generates boardless, and the build prints "✓ build succeeded" for a tree
  // with no board in it.
  assert.throws(() => parseBoardYaml('chip: esp32c6\n'), /'selected' is required/)
  assert.throws(() => parseBoardYaml('selected: x\n'), /'chip' is required/)
  assert.throws(() => parseBoardYaml('schema: zc-board/9\nselected: x\nchip: c6\n'), /schema must be/)
  assert.throws(
    () => parseBoardYaml('selected: x\nchip: c6\nbmgr:\n  board: b\n  resolved_from: guessed\n'),
    /resolved_from must be one of/,
  )
  // The shape the two samples use round-trips.
  const b = parseBoardYaml('schema: zc-board/1\nselected: esp32-c6-devkitc-1\nchip: esp32c6\nbmgr:\n  board: esp32_c6_devkitc_1\n  resolved_from: exact\n')
  assert.deepEqual(b, { selected: 'esp32-c6-devkitc-1', chip: 'esp32c6', schema: 'zc-board/1', bmgr: { board: 'esp32_c6_devkitc_1', resolved_from: 'exact' } })
})
