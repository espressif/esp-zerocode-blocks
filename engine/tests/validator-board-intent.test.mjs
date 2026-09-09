#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * The three INTENT rules on the board path — the checks that tell "collides
 * with the board" apart from "deliberately drives the board's hardware":
 *
 *  1. STEP-ASIDE (validator side of the generator's provided_by logic): a
 *     block whose slots the board replaces contributes no pins, so its cfg
 *     pins are exempt from the board pin-conflict check. On a board WITHOUT
 *     the device the block keeps its init and its pins conflict-check
 *     normally. One predicate (boardTakesOver) shared with renderInstances.
 *
 *  2. ADAPTER BINDING: an instance binding a board device by name
 *     (provided_by + device_param) exempts the pins that device claims; and
 *     a plain-pin conflict with a bindable device names the adapter as the
 *     fix.
 *
 *  3. PROVIDER-SIDE BUS COLLISION: a provides_bus instance on a port the
 *     board already provides is an error (the boot would abort in
 *     esp_board_manager_init) — in the validator AND in buildBoardAmend, for
 *     a product generated without validation.
 *
 * Standalone: node's test runner against engine/dist, temp board packs.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { mkdtempSync, mkdirSync, writeFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const { validateProduct, buildBoardAmend } = await import(join(ROOT, 'engine/dist/index.js'))

/* ── fixtures ────────────────────────────────────────────────────────── */

const BLOCKS = new Map([
  // A hand-pinned panel the board may already carry (step-aside shape).
  ['drivers/fx_panel', {
    id: 'drivers/fx_panel',
    kind: 'driver',
    description: 'panel',
    params: { sclk_gpio: { type: 'int' }, cs_gpio: { type: 'int' } },
    bmgr: { init: 'board_manager', provided_by: 'display_lcd', replaces_slots: ['display_node_init'] },
  }],
  // A part wired TO the board (amend shape — no provided_by): its pins are
  // real claims and must stay inside the conflict check.
  ['drivers/fx_relay', {
    id: 'drivers/fx_relay',
    kind: 'driver',
    description: 'relay',
    params: { gpio: { type: 'int' } },
    bmgr: {
      init: 'board_manager',
      replaces_slots: ['driver_init'],
      peripherals: [{ name: 'gpio_{{prefix_lc}}', type: 'gpio', config: { pin: '{{cfg.gpio}}' } }],
    },
  }],
  // A board-device ADAPTER for buttons (board_button shape).
  ['drivers/fx_button_adapter', {
    id: 'drivers/fx_button_adapter',
    kind: 'driver',
    description: 'adapter',
    params: { device: { type: 'string', required: true } },
    bmgr: { init: 'board_manager', provided_by: 'button', device_param: 'device' },
  }],
  // A behavior claiming a raw pin (the long-press-on-the-same-button case).
  ['behaviors/fx_longpress', {
    id: 'behaviors/fx_longpress',
    kind: 'behavior',
    description: 'longpress',
    params: { gpio: { type: 'int' } },
  }],
  // A bus PROVIDER (peripherals/i2c_bus shape), with the same bmgr fragment.
  ['peripherals/fx_i2c_bus', {
    id: 'peripherals/fx_i2c_bus',
    kind: 'peripheral',
    description: 'bus',
    params: { port: { type: 'int', default: 0 }, sda_gpio: { type: 'int' }, scl_gpio: { type: 'int' } },
    provides_bus: { type: 'i2c', port_param: 'port' },
    bmgr: {
      init: 'board_manager',
      replaces_slots: ['driver_init'],
      peripherals: [{
        shared_key: 'i2c-{{cfg.port}}',
        name: 'i2c_zc_{{cfg.port}}',
        type: 'i2c',
        config: { port: '{{cfg.port}}', pins: { sda: '{{cfg.sda_gpio}}', scl: '{{cfg.scl_gpio}}' } },
      }],
    },
  }],
])

/** Board: an i2c bus on port 0 (pins 21/22), a BOOT button device backed by a
 *  gpio peripheral on pin 9, and a display_lcd device (cs 31). */
const PERIPHERALS = `
peripherals:
  - name: i2c_master
    type: i2c
    config:
      port: 0
      pins:
        sda: 21
        scl: 22
  - name: gpio_boot_button
    type: gpio
    config:
      pin: 9
      mode: GPIO_MODE_INPUT
  - name: spi_display
    type: spi
    config:
      spi_bus_config:
        spi_port: SPI2_HOST
        sclk_io_num: 30
`

const DEVICES = `
devices:
  - name: boot_button
    type: button
    config:
      active_level: 0
    peripherals:
      - gpio_name: gpio_boot_button
  - name: display_lcd
    type: display_lcd
    config:
      io_spi_config:
        cs_gpio_num: 31
    peripherals:
      - spi_name: spi_display
`

/** The same board with NO devices at all — a bare devkit. */
const BARE_DEVICES = 'devices: []\n'

function makePack({ board = 'fx_board', chip = 'esp32s31', peripherals = PERIPHERALS, devices = DEVICES } = {}) {
  const root = mkdtempSync(join(tmpdir(), 'zc-boards-'))
  const dir = join(root, 'fx_pack', board)
  mkdirSync(dir, { recursive: true })
  writeFileSync(join(dir, 'board_info.yaml'), `board: ${board}\nchip: ${chip}\n`)
  writeFileSync(join(dir, 'board_peripherals.yaml'), peripherals)
  writeFileSync(join(dir, 'board_devices.yaml'), devices)
  return root
}

const cleanup = []
function pack(opts) {
  const root = makePack(opts)
  cleanup.push(root)
  return root
}
test.after(() => { for (const r of cleanup) rmSync(r, { recursive: true, force: true }) })

function boardFile() {
  return { schema: 'zc-board/1', selected: 'fx-devkit', chip: 'esp32s31', bmgr: { board: 'fx_board', resolved_from: 'exact' } }
}

function product(instances) {
  return { id: 'fixture-product', name: 'Fixture', description: 'fixture', keywords: [], frameworks: [], instances }
}

const errs = (r) => r.errors.map((i) => i.message)

/* ── 1. step-aside: the validator agrees with the generator ──────────── */

test('a provided_by block on a board that carries the device: its pins are NOT conflicts', () => {
  const root = pack({})
  // fx_panel hand-pins the board's own display wiring (sclk 30, cs 31).
  const p = product([{ block: 'drivers/fx_panel', prefix: 'PANEL', cfg: { sclk_gpio: 30, cs_gpio: 31 } }])
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.deepEqual(errs(r), [])
})

test('the SAME block on a board WITHOUT the device keeps its pins in the check', () => {
  // A panel wired externally to a bare devkit: the block keeps its init (the
  // generator side) and its pins conflict-check normally (this side).
  const root = pack({ devices: BARE_DEVICES })
  const p = product([{ block: 'drivers/fx_panel', prefix: 'PANEL', cfg: { sclk_gpio: 30, cs_gpio: 31 } }])
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /gpio 30/)
  assert.match(r.errors[0].message, /spi_display/)
})

test('a mapping with NO provided_by (an amend part) never gets the exemption', () => {
  // fx_relay's pin becomes an amend claim on any board, so a collision with
  // the board is real whichever devices the board carries.
  const root = pack({})
  const p = product([{ block: 'drivers/fx_relay', prefix: 'APP', cfg: { gpio: 9 } }])
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /gpio 9/)
})

test('a step-aside block does not shadow another instance claiming the same pin', () => {
  // pinClaims is per-instance on purpose: the panel's exempt cs on 31 must
  // not hide the relay's REAL claim on 31.
  const root = pack({})
  const p = product([
    { block: 'drivers/fx_panel', prefix: 'PANEL', cfg: { sclk_gpio: 30, cs_gpio: 31 } },
    { block: 'drivers/fx_relay', prefix: 'APP', cfg: { gpio: 31 } },
  ])
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /drivers\/fx_relay/)
  assert.match(r.errors[0].message, /gpio 31/)
})

/* ── 2. adapter binding ──────────────────────────────────────────────── */

test('a pin conflict with a bindable board device NAMES the adapter as the fix', () => {
  const root = pack({})
  const p = product([{ block: 'behaviors/fx_longpress', prefix: 'FR', cfg: { gpio: 9 } }])
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.equal(r.errors.length, 1)
  const m = r.errors[0].message
  assert.match(m, /gpio 9/)
  assert.match(m, /boot_button/)
  assert.match(m, /drivers\/fx_button_adapter \(cfg\.device: boot_button\)/)
})

test('binding the claiming device through an adapter EXEMPTS its pins', () => {
  // The long-press behavior shares the bound button's GPIO — asking the
  // owner, not fighting it.
  const root = pack({})
  const p = product([
    { block: 'drivers/fx_button_adapter', prefix: 'BTN', cfg: { device: 'boot_button' } },
    { block: 'behaviors/fx_longpress', prefix: 'FR', cfg: { gpio: 9 } },
  ])
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.deepEqual(errs(r), [])
})

test('binding a DIFFERENT device exempts nothing', () => {
  const root = pack({
    devices: DEVICES + `  - name: side_button
    type: button
    config:
      active_level: 0
`,
  })
  const p = product([
    { block: 'drivers/fx_button_adapter', prefix: 'BTN', cfg: { device: 'side_button' } },
    { block: 'behaviors/fx_longpress', prefix: 'FR', cfg: { gpio: 9 } },
  ])
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.equal(errs(r).filter((m) => /gpio 9/.test(m)).length, 1)
})

/* ── 3. provider-side bus collision ──────────────────────────────────── */

test('a provides_bus instance on a board-provided port is an ERROR naming the peripheral', () => {
  const root = pack({})
  const p = product([{ block: 'peripherals/fx_i2c_bus', prefix: 'BUS', cfg: { port: 0, sda_gpio: 17, scl_gpio: 18 } }])
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  const bus = errs(r).filter((m) => /create the bus twice/.test(m))
  assert.equal(bus.length, 1)
  assert.match(bus[0], /i2c port 0/)
  assert.match(bus[0], /'i2c_master'/)
  assert.match(bus[0], /adopt the board's bus/)
})

test('a provider on a port the board does NOT provide is fine', () => {
  const root = pack({})
  const p = product([{ block: 'peripherals/fx_i2c_bus', prefix: 'BUS', cfg: { port: 1, sda_gpio: 17, scl_gpio: 18 } }])
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.deepEqual(errs(r), [])
})

test('an unreadable board pack downgrades the provider check to a warning', () => {
  const p = product([{ block: 'peripherals/fx_i2c_bus', prefix: 'BUS', cfg: { port: 0, sda_gpio: 17, scl_gpio: 18 } }])
  const r = validateProduct(p, BLOCKS,
    { boardsDir: join(tmpdir(), 'zc-boards-does-not-exist-67890'), board: boardFile() })
  assert.deepEqual(errs(r), [])
  assert.equal(r.warnings.filter((w) => /collision with a board-owned bus was not checked/.test(w.message)).length, 1)
})

test('buildBoardAmend refuses a bus fragment on a board-provided port', () => {
  const root = pack({})
  const boardDir = join(root, 'fx_pack', 'fx_board')
  const rendered = [{
    block: BLOCKS.get('peripherals/fx_i2c_bus'),
    prefix: 'BUS',
    cfg: { port: 0, sda_gpio: 17, scl_gpio: 18 },
    slots: {},
  }]
  assert.throws(
    () => buildBoardAmend(product([]), rendered, 'fx_board', boardDir),
    (e) => /create the bus twice/.test(e.message) && /'i2c_master'/.test(e.message) && /drop the peripherals\/fx_i2c_bus instance/i.test(e.message),
  )
})

test('buildBoardAmend keeps emitting a bus on a free port (and with no boardDir)', () => {
  const root = pack({})
  const boardDir = join(root, 'fx_pack', 'fx_board')
  const mk = (port, dir) => buildBoardAmend(product([]), [{
    block: BLOCKS.get('peripherals/fx_i2c_bus'),
    prefix: 'BUS',
    cfg: { port, sda_gpio: 17, scl_gpio: 18 },
    slots: {},
  }], 'fx_board', dir)
  assert.match(mk(1, boardDir).fragment, /port: 1/)
  // No pack on this machine: the guard skips, same stance as every board check.
  assert.match(mk(0, null).fragment, /port: 0/)
})
