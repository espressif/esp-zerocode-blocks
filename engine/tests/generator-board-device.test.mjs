#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Unit tests for the board-DEVICE adapter (`bmgr.device_param`): a block that
 * drives hardware the board already owns — its status LED, a vibration motor,
 * an onboard relay — by asking esp-board-manager for the handle.
 *
 * What each test is really guarding:
 *
 *   - the block states NO pin. That is the whole mechanism: the board claims
 *     the line for its own device, so a product claiming it too is an IO
 *     conflict, and the way to use someone else's pin is to ask them;
 *   - the device is matched by TYPE and fetched by NAME, and both halves
 *     matter — a board may carry SEVERAL devices of one type, so "the first
 *     gpio_ctrl" is not an answer;
 *   - all three ways of getting it wrong (no board, unreadable pack, no such
 *     device) fail at GENERATION. Unchecked, every one of them produces a tree
 *     that compiles, links and boots, and reports itself in a single ESP_LOGE
 *     at startup — the failure shape the bus rules exist to refuse;
 *   - the validator says the same thing earlier, so a whole catalog is checked
 *     at once rather than one generate at a time.
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
const { generate, validateProduct } = await import(join(ROOT, 'engine/dist/index.js'))

const APP_MAIN = `#include "app_driver.h"
{{main_includes}}

extern "C" void app_main(void)
{
{{board_init}}
    ESP_ERROR_CHECK(app_driver_init());
{{main_init}}
}
`

/** Two gpio_ctrl devices, which is the realistic case and the reason the
 *  adapter is addressed by name: the ESP-Mosaico carries exactly this pair. */
const TWO_GPIO_CTRL = [
  'devices:',
  '  - name: status_led',
  '    type: gpio_ctrl',
  '  - name: vibration_motor',
  '    type: gpio_ctrl',
  '',
].join('\n')

function scaffold(t, { boardDevices } = {}) {
  const root = mkdtempSync(join(tmpdir(), 'zc-gen-dev-'))
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
  const writeBlock = (id, kind, body) => {
    mkdirSync(join(blocks, id), { recursive: true })
    writeFileSync(join(blocks, id, 'block.yml'), JSON.stringify({ id, kind, ...body }, null, 2))
  }

  // The adapter under test, in the shape drivers/board_gpio_ctrl has.
  writeBlock('drivers/demo_board_gpio', 'driver', {
    description: 'drive a board-declared gpio_ctrl device',
    params: {
      device: { type: 'string', required: true },
      // driver_params are assembled into params + param_refs — see
      // scripts/assemble_blocks.py; the engine only ever sees this shape.
      param_id: { type: 'string', default: 'APP_DRIVER_PARAM_POWER' },
    },
    param_refs: { param_id: 'bool' },
    bmgr: { init: 'board_manager', provided_by: 'gpio_ctrl', device_param: 'device' },
    slots: {
      driver_init: '{\n    esp_board_manager_get_device_handle("{{cfg.device}}", &{{prefix_lc}}_h);\n}',
      driver_apply_cases: 'case {{cfg.param_id}}: return {{prefix_lc}}_set(val.b);',
    },
  })

  // A block that wires its OWN pin, for the contrast: this one names a GPIO
  // and needs no board at all.
  writeBlock('drivers/demo_relay', 'driver', {
    description: 'externally wired relay',
    params: { gpio: { type: 'int', required: true }, param_id: { type: 'string', default: 'APP_DRIVER_PARAM_POWER' } },
    param_refs: { param_id: 'bool' },
    slots: { config_defines: '#define {{prefix}}_RELAY_GPIO {{cfg.gpio}}' },
  })

  const boards = join(root, 'boards')
  const boardDir = join(boards, 'pack_one', 'demo_board')
  mkdirSync(boardDir, { recursive: true })
  writeFileSync(join(boardDir, 'board_info.yaml'), 'board: demo_board\nchip: esp32s3\n')
  writeFileSync(join(boardDir, 'sdkconfig.defaults.board'), 'CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y\n')
  if (boardDevices !== undefined) writeFileSync(join(boardDir, 'board_devices.yaml'), boardDevices)

  return {
    paths: { templatesDir: templates, baseFirmwareDir: base, boardsDir: boards },
    boardsDir: boards,
    outDir: join(root, 'out'),
  }
}

const product = (instances) => ({
  id: 'demo', name: 'demo', description: 'demo', frameworks: [], instances,
})

const ledInstance = (device = 'status_led') => ({
  block: 'drivers/demo_board_gpio',
  prefix: 'LAMP',
  cfg: { device, param_id: 'APP_DRIVER_PARAM_POWER' },
})

const boardFile = (bmgrBoard) => ({
  schema: 'zc-board/1',
  selected: 'demo-devkit',
  chip: 'esp32s3',
  bmgr: bmgrBoard === null ? null : { board: bmgrBoard, resolved_from: 'exact' },
})

const appDriver = (outDir) =>
  readFileSync(join(outDir, 'components/app_driver/app_driver.cpp'), 'utf-8')

/* ── the adapter ─────────────────────────────────────────────────────── */

test('a block can drive a board-declared gpio_ctrl device, by name and with no pin', async (t) => {
  const { paths, outDir } = scaffold(t, { boardDevices: TWO_GPIO_CTRL })
  await generate(paths, {
    product: product([ledInstance('status_led')]),
    board: boardFile('demo_board'),
    chip: 'esp32s3',
    outDir,
  })
  const cpp = appDriver(outDir)
  assert.match(cpp, /esp_board_manager_get_device_handle\("status_led"/)
  // NO PIN. The board owns the line; a GPIO number here would be a second
  // claim on it, which is exactly what this adapter exists to avoid.
  assert.ok(!/gpio_config|GPIO_MODE_OUTPUT/.test(cpp), 'the adapter configures no pin')
  // The board comes up first, or the handle it fetches does not exist yet.
  const main = readFileSync(join(outDir, 'main/app_main.cpp'), 'utf-8')
  assert.ok(main.indexOf('esp_board_manager_init()') < main.indexOf('app_driver_init()'))
})

test('the device is picked by NAME, not by being the first of its type', async (t) => {
  // The board declares status_led FIRST. A "first device of this type" rule
  // would silently drive the LED when the product asked for the motor — a
  // board with several devices of one type is the normal case, not a corner.
  const { paths, outDir } = scaffold(t, { boardDevices: TWO_GPIO_CTRL })
  await generate(paths, {
    product: product([ledInstance('vibration_motor')]),
    board: boardFile('demo_board'),
    chip: 'esp32s3',
    outDir,
  })
  const cpp = appDriver(outDir)
  assert.match(cpp, /esp_board_manager_get_device_handle\("vibration_motor"/)
  assert.ok(!cpp.includes('"status_led"'), 'it drove the device the product named')
})

/* ── the three ways to get it wrong, all refused at generation ───────── */

test('a board device the board does not declare fails GENERATION', async (t) => {
  const { paths, outDir } = scaffold(t, { boardDevices: TWO_GPIO_CTRL })
  await assert.rejects(
    generate(paths, {
      product: product([ledInstance('backlight')]),
      board: boardFile('demo_board'),
      chip: 'esp32s3',
      outDir,
    }),
    // The message must name what the board DOES have — the usual mistake is a
    // spelling, and a bare "not found" makes the author go read the pack.
    /'backlight'.*'gpio_ctrl'.*'status_led'.*'vibration_motor'/s,
  )
})

test('a device of the RIGHT name but the WRONG type fails GENERATION', async (t) => {
  // Names are not unique across types, and the type decides which handle
  // struct comes back — a button fetched as a gpio_ctrl is a bad cast.
  const { paths, outDir } = scaffold(t, {
    boardDevices: 'devices:\n  - name: status_led\n    type: button\n',
  })
  await assert.rejects(
    generate(paths, {
      product: product([ledInstance('status_led')]),
      board: boardFile('demo_board'),
      chip: 'esp32s3',
      outDir,
    }),
    /type 'gpio_ctrl'.*does not declare/s,
  )
})

test('a product with NO board cannot use a board-device adapter', async (t) => {
  const { paths, outDir } = scaffold(t, { boardDevices: TWO_GPIO_CTRL })
  await assert.rejects(
    generate(paths, { product: product([ledInstance()]), board: null, chip: 'esp32s3', outDir }),
    /has no board/,
  )
})

test('a board whose pack cannot be read fails GENERATION rather than guessing', async (t) => {
  // Same stance as the display adapter: an unreadable pack is not permission
  // to generate a fetch for a device nobody has confirmed exists.
  const { paths, outDir } = scaffold(t, { boardDevices: TWO_GPIO_CTRL })
  await assert.rejects(
    generate(paths, {
      product: product([ledInstance()]),
      board: boardFile('no_such_board'),
      chip: 'esp32s3',
      outDir,
    }),
    /pack could not be read/,
  )
})

test('a block that owns its own pin is untouched by any of this', async (t) => {
  // The adapter is a new shape, not a new rule for everyone: a block with no
  // bmgr.device_param generates on a board product exactly as it always did.
  const { paths, outDir } = scaffold(t, { boardDevices: TWO_GPIO_CTRL })
  await generate(paths, {
    product: product([{ block: 'drivers/demo_relay', prefix: 'R', cfg: { gpio: 4, param_id: 'APP_DRIVER_PARAM_POWER' } }]),
    board: boardFile('demo_board'),
    chip: 'esp32s3',
    outDir,
  })
  const cfgH = readFileSync(join(outDir, 'components/app_config/include/app_config.h'), 'utf-8')
  assert.match(cfgH, /R_RELAY_GPIO 4/)
})

/* ── the validator says the same thing, earlier ──────────────────────── */

function validate(t, { boardDevices, device, board }) {
  const { boardsDir } = scaffold(t, { boardDevices })
  const blocks = new Map([['drivers/demo_board_gpio', {
    id: 'drivers/demo_board_gpio',
    kind: 'driver',
    description: 'adapter',
    params: {
      device: { type: 'string', required: true },
      // driver_params are assembled into params + param_refs — see
      // scripts/assemble_blocks.py; the engine only ever sees this shape.
      param_id: { type: 'string', default: 'APP_DRIVER_PARAM_POWER' },
    },
    param_refs: { param_id: 'bool' },
    bmgr: { init: 'board_manager', provided_by: 'gpio_ctrl', device_param: 'device' },
  }]])
  return validateProduct(product([ledInstance(device)]), blocks, { boardsDir, board })
}

test('the validator rejects a board device the board does not declare', (t) => {
  const r = validate(t, { boardDevices: TWO_GPIO_CTRL, device: 'backlight', board: boardFile('demo_board') })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /'backlight'.*'gpio_ctrl'.*'status_led'/s)
})

test('the validator accepts a device the board really declares', (t) => {
  const r = validate(t, { boardDevices: TWO_GPIO_CTRL, device: 'status_led', board: boardFile('demo_board') })
  assert.deepEqual(r.errors, [])
})

test('the validator rejects a board-device adapter in a boardless product', (t) => {
  const r = validate(t, { boardDevices: TWO_GPIO_CTRL, device: 'status_led', board: null })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /has no board/)
})

test('an unreadable board pack WARNS rather than erroring in the validator', (t) => {
  // Host content missing is not a product defect — the same stance every other
  // board rule takes. The generator is where it becomes a refusal.
  const r = validate(t, { boardDevices: TWO_GPIO_CTRL, device: 'status_led', board: boardFile('no_such_board') })
  assert.ok(r.errors.every(e => !/wants board device/.test(e.message)), 'no adapter error')
  assert.ok(r.warnings.some(w => /was not checked/.test(w.message)), 'it says it did not check')
})
