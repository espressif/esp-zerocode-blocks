#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Unit tests for PHASE 3 of the board-manager switch-over: a board that
 * declares its own panel supplies the display framework's hardware, and the
 * driver block that used to hand-pin that panel steps aside.
 *
 * What each test is really guarding:
 *
 *   - the adapter exists at all, and it FETCHES rather than initializes — a
 *     second bring-up of a panel the board already made is not a cosmetic
 *     duplication, it is spi_bus_initialize() returning ESP_ERR_INVALID_STATE
 *     into an ESP_ERROR_CHECK at boot;
 *   - `provided_by` is a GATE, not a switch: the same panel block wired to a
 *     board with no panel of its own keeps its init, or a product silently
 *     loses its screen;
 *   - a display product with nothing providing a panel fails at GENERATION.
 *     The failure it replaces is a dark screen on a bench;
 *   - the board is matched on device TYPE and called by device NAME, because
 *     those are two different things that usually look alike.
 *
 * Standalone like its siblings: node's runner against engine/dist, with a
 * base_firmware + catalog + board pack built in a temp dir, so nothing here
 * needs a board-pack checkout.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const { generate } = await import(join(ROOT, 'engine/dist/index.js'))

const APP_MAIN = `#include "app_driver.h"
{{main_includes}}

extern "C" void app_main(void)
{
{{board_init}}
    ESP_ERROR_CHECK(app_driver_init());
{{main_init}}
}
`

/** The panel block's own bring-up, verbatim enough to be recognizable in the
 *  generated file: the point of the collapse is that this text disappears. */
const PANEL_NODE_INIT = `{
    spi_bus_config_t {{prefix_lc}}_bus = {};
    {{prefix_lc}}_bus.sclk_io_num = {{prefix}}_LCD_SCLK;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &{{prefix_lc}}_bus, SPI_DMA_CH_AUTO));
    s_disp = lvgl_port_add_disp(&{{prefix_lc}}_disp_cfg);
}`

function scaffold(t, { boardDevices } = {}) {
  const root = mkdtempSync(join(tmpdir(), 'zc-gen-disp-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))

  const base = join(root, 'base_firmware')
  mkdirSync(join(base, 'main'), { recursive: true })
  writeFileSync(join(base, 'main/app_main.cpp'), APP_MAIN)
  writeFileSync(join(base, 'main/idf_component.yml'), 'dependencies: {}\n')
  writeFileSync(join(base, 'partitions.csv'), 'factory, app, factory, 0x10000, 0x100000,\n')
  writeFileSync(join(base, 'sdkconfig.defaults'), '# base\n')

  const templates = join(root, 'templates')
  const blocks = join(templates, 'code_blocks')
  mkdirSync(join(templates, 'product_configurations'), { recursive: true })

  // Blocks are written in the ASSEMBLED shape the engine loads — id, kind and
  // slots inline, the way scripts/assemble_blocks.py emits them.
  const writeBlock = (id, kind, body) => {
    mkdirSync(join(blocks, id), { recursive: true })
    writeFileSync(join(blocks, id, 'block.yml'), JSON.stringify({ id, kind, ...body }, null, 2))
  }

  // The display FRAMEWORK block. It carries no cfg, which is the whole reason
  // panel pins ended up in a driver block in the first place.
  writeBlock('frameworks/display', 'framework', {
    description: 'display framework',
    slots: { main_init: 'ESP_ERROR_CHECK(app_display_init());' },
  })

  // The panel DRIVER block, with the phase-3 mapping.
  writeBlock('drivers/demo_panel', 'driver', {
    description: 'demo SPI panel',
    params: { sclk_gpio: { type: 'int', required: true } },
    bmgr: {
      init: 'board_manager',
      provided_by: 'display_lcd',
      replaces_slots: ['display_node_init', 'display_includes', 'config_defines'],
    },
    slots: {
      display_node_init: PANEL_NODE_INIT,
      display_includes: '#include <esp_lcd_panel_io.h>',
      config_defines: '#define {{prefix}}_LCD_SCLK {{cfg.sclk_gpio}}',
    },
  })

  // One device type, so the framework component's CMakeLists is generated —
  // a display product with no card on it is not a display product.
  writeBlock('device_types/demo_card', 'device_type', {
    description: 'demo card',
    slots: { display_widget_create: '{\n    lv_label_create(parent);\n}' },
  })

  // A board pack in the esp-board-manager shape.
  const boards = join(root, 'boards')
  const boardDir = join(boards, 'pack_one', 'demo_board')
  mkdirSync(boardDir, { recursive: true })
  writeFileSync(join(boardDir, 'board_info.yaml'), 'board: demo_board\nchip: esp32s3\n')
  writeFileSync(join(boardDir, 'sdkconfig.defaults.board'), 'CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y\n')
  if (boardDevices !== undefined) {
    writeFileSync(join(boardDir, 'board_devices.yaml'), boardDevices)
  }

  return {
    paths: { templatesDir: templates, baseFirmwareDir: base, boardsDir: boards },
    outDir: join(root, 'out'),
  }
}

/** A board carrying a panel and a touch layer, bmgr's canonical names. */
const PANEL_AND_TOUCH = [
  'devices:',
  '  - name: display_lcd',
  '    type: display_lcd',
  '    chip: co5300',
  '  - name: lcd_touch',
  '    type: lcd_touch',
  '    chip: cst9217',
  '',
].join('\n')

const product = (over = {}) => ({
  id: 'demo', name: 'demo', description: 'demo',
  frameworks: ['display'], instances: [], ...over,
})

const panelInstance = { block: 'drivers/demo_panel', prefix: 'PANEL', cfg: { sclk_gpio: 44 } }
const cardInstance = { block: 'device_types/demo_card', prefix: 'CARD', cfg: {} }

const boardFile = (bmgrBoard) => ({
  schema: 'zc-board/1',
  selected: 'demo-devkit',
  chip: 'esp32s3',
  bmgr: bmgrBoard === null ? null : { board: bmgrBoard, resolved_from: 'exact' },
})

const appDisplay = (outDir) =>
  readFileSync(join(outDir, 'components/app_display/app_display.cpp'), 'utf-8')

/* ── the adapter ─────────────────────────────────────────────────────── */

test('a board that declares a panel supplies it to the display framework', async (t) => {
  const { paths, outDir } = scaffold(t, { boardDevices: PANEL_AND_TOUCH })
  await generate(paths, {
    product: product({ instances: [cardInstance] }),
    board: boardFile('demo_board'),
    chip: 'esp32s3',
    outDir,
  })
  const cpp = appDisplay(outDir)

  assert.match(cpp, /esp_board_manager_get_device_handle\("display_lcd"/)
  assert.match(cpp, /esp_board_manager_get_device_config\("display_lcd"/)
  assert.match(cpp, /s_disp = lvgl_port_add_disp\(&board_disp_cfg\)/)
  assert.match(cpp, /esp_board_manager_get_device_handle\("lcd_touch"/)
  assert.match(cpp, /lvgl_port_add_touch\(&board_touch_cfg\)/)
  // FETCHES, does not initialize. A second bring-up of the board's own panel
  // is an abort at boot, not a redundancy.
  assert.ok(!cpp.includes('spi_bus_initialize'), 'the adapter never touches the bus')
  assert.ok(!cpp.includes('esp_lcd_new_panel'), 'the adapter never creates a panel')

  // The framework component has to be able to SEE the board manager's headers.
  const cmake = readFileSync(join(outDir, 'components/app_display/CMakeLists.txt'), 'utf-8')
  assert.match(cmake, /espressif__esp_board_manager/)
})

test('the panel comes up before app_display_init(), because the board does', async (t) => {
  const { paths, outDir } = scaffold(t, { boardDevices: PANEL_AND_TOUCH })
  await generate(paths, { product: product(), board: boardFile('demo_board'), chip: 'esp32s3', outDir })
  const main = readFileSync(join(outDir, 'main/app_main.cpp'), 'utf-8')
  // The adapter fetches a handle; the handle exists only after board init. If
  // these two ever swap, the fetch returns nothing and the screen stays dark.
  assert.ok(main.indexOf('esp_board_manager_init()') < main.indexOf('app_display_init()'))
})

/* ── provided_by is a gate ───────────────────────────────────────────── */

test('a panel block STEPS ASIDE on a board that already has a panel', async (t) => {
  const { paths, outDir } = scaffold(t, { boardDevices: PANEL_AND_TOUCH })
  await generate(paths, {
    product: product({ instances: [panelInstance] }),
    board: boardFile('demo_board'),
    chip: 'esp32s3',
    outDir,
  })
  const cpp = appDisplay(outDir)
  assert.ok(!cpp.includes('spi_bus_initialize'), "the block's own bring-up is gone")
  assert.match(cpp, /esp_board_manager_get_device_handle\("display_lcd"/)
  // config_defines went with it: nothing is left to reference PANEL_LCD_SCLK.
  const cfgH = readFileSync(join(outDir, 'components/app_config/include/app_config.h'), 'utf-8')
  assert.ok(!cfgH.includes('PANEL_LCD_SCLK'), 'the block states no pins it no longer drives')
})

test('a panel block KEEPS its init on a board with no panel of its own', async (t) => {
  // Same block, same board product — the board just does not carry a display.
  // This is the externally-wired case, and dropping the block's init here
  // would cost the product its screen with nothing in the tree to show why.
  const { paths, outDir } = scaffold(t, { boardDevices: 'devices: []\n' })
  await generate(paths, {
    product: product({ instances: [panelInstance] }),
    board: boardFile('demo_board'),
    chip: 'esp32s3',
    outDir,
  })
  const cpp = appDisplay(outDir)
  assert.match(cpp, /spi_bus_initialize/)
  assert.ok(!cpp.includes('esp_board_manager_get_device_handle'), 'no adapter, nothing to fetch')
  const cfgH = readFileSync(join(outDir, 'components/app_config/include/app_config.h'), 'utf-8')
  assert.match(cfgH, /PANEL_LCD_SCLK 44/)
})

test('a boardless product is untouched by any of this', async (t) => {
  const { paths, outDir } = scaffold(t, { boardDevices: PANEL_AND_TOUCH })
  await generate(paths, { product: product({ instances: [panelInstance] }), board: null, chip: 'esp32s3', outDir })
  const cpp = appDisplay(outDir)
  assert.match(cpp, /spi_bus_initialize/)
  assert.ok(!cpp.includes('esp_board_manager'), 'nothing board-shaped leaks into app_display')
  // app_main too. The display file alone is not enough evidence: the board
  // contributes nothing to it when the board carries no panel, so a board
  // leaking into EVERY product would still leave app_display.cpp clean.
  const main = readFileSync(join(outDir, 'main/app_main.cpp'), 'utf-8')
  assert.ok(!main.includes('esp_board_manager'), 'nor into app_main')
})

/* ── the board is matched on TYPE, called by NAME ────────────────────── */

test('a board naming its panel something else is still found, and called by that name', async (t) => {
  const { paths, outDir } = scaffold(t, {
    boardDevices: [
      'devices:',
      '  - name: display_lcd_0',
      '    type: display_lcd',
      '',
    ].join('\n'),
  })
  await generate(paths, { product: product(), board: boardFile('demo_board'), chip: 'esp32s3', outDir })
  const cpp = appDisplay(outDir)
  assert.match(cpp, /esp_board_manager_get_device_handle\("display_lcd_0"/)
  // No touch device on this board, so nothing pretends there is one.
  assert.ok(!cpp.includes('lvgl_port_add_touch'), 'a touch the board does not declare is not invented')
})

/* ── nothing provides a panel ────────────────────────────────────────── */

test('a display product with no panel at all fails GENERATION, not the bench', async (t) => {
  const { paths, outDir } = scaffold(t, { boardDevices: 'devices: []\n' })
  await assert.rejects(
    generate(paths, { product: product(), board: boardFile('demo_board'), chip: 'esp32s3', outDir }),
    (e) => {
      assert.match(e.message, /nothing provides a panel/)
      assert.match(e.message, /Board 'demo_board' declares no display_lcd device/)
      return true
    },
  )
})

test('an unreachable board pack says so in the same failure', async (t) => {
  const { paths, outDir } = scaffold(t, { boardDevices: PANEL_AND_TOUCH })
  await assert.rejects(
    // A board name no pack defines: we cannot know whether it has a panel, and
    // the honest answer is not "generate a product with no screen".
    generate(paths, { product: product(), board: boardFile('not_in_any_pack'), chip: 'esp32s3', outDir }),
    (e) => {
      assert.match(e.message, /nothing provides a panel/)
      assert.match(e.message, /pack could not be read/)
      return true
    },
  )
})
