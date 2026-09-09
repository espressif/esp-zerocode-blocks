#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Unit tests for the validator's BOARD checks (esp-board-manager).
 *
 * The board is a SEPARATE input — a parsed board.yaml describing the hardware
 * one product instance runs on — never a product key. So every check here
 * runs only when a board file is supplied, and a catalog product on its own is
 * general: nothing below fires for it.
 *
 * Standalone, like scripts/validate.mjs: node's built-in test runner against
 * the built engine (engine/dist), no monorepo and no extra dependency.
 *   npm test        (builds first)
 *   node --test engine/tests/
 *
 * Every test builds its own board pack in a temp dir and points the validator
 * at it with `{ boardsDir }`, so nothing here depends on a board-pack checkout
 * being checked out.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { mkdtempSync, mkdirSync, writeFileSync, rmSync, symlinkSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const { validateProduct } = await import(join(ROOT, 'engine/dist/index.js'))

/* ── fixtures ────────────────────────────────────────────────────────── */

/** One block whose cfg carries two GPIO params. */
const BLOCKS = new Map([
  ['drivers/relay', {
    id: 'drivers/relay',
    kind: 'driver',
    description: 'relay',
    params: { gpio: { type: 'u8' }, sense_gpio: { type: 'u8' } },
  }],
])

function product(over = {}) {
  return {
    id: 'fixture-product',
    name: 'Fixture',
    description: 'fixture',
    keywords: [],
    frameworks: [],
    instances: [{ block: 'drivers/relay', prefix: 'RELAY', cfg: { gpio: 20 } }],
    ...over,
  }
}

/** Write a board pack: <root>/<pack>/<board>/{board_info,board_peripherals,
 *  board_devices}.yaml. Returns the root. */
function makePack({ board = 'fx_board', chip = 'esp32s31', peripherals, devices } = {}) {
  const root = mkdtempSync(join(tmpdir(), 'zc-boards-'))
  const dir = join(root, 'fx_pack', board)
  mkdirSync(dir, { recursive: true })
  writeFileSync(join(dir, 'board_info.yaml'), `board: ${board}\nchip: ${chip}\n`)
  if (peripherals !== undefined) writeFileSync(join(dir, 'board_peripherals.yaml'), peripherals)
  if (devices !== undefined) writeFileSync(join(dir, 'board_devices.yaml'), devices)
  return root
}

const cleanup = []
function pack(opts) {
  const root = makePack(opts)
  cleanup.push(root)
  return root
}
test.after(() => { for (const r of cleanup) rmSync(r, { recursive: true, force: true }) })

/** A board file: what the user picked, and the bmgr board it resolves to. */
function boardFile({ bmgr = 'fx_board', chip = 'esp32s31', selected = 'fx-devkit', resolved_from = 'exact' } = {}) {
  return {
    schema: 'zc-board/1',
    selected,
    chip,
    bmgr: bmgr === undefined ? undefined : { board: bmgr, resolved_from },
  }
}

const msgs = (r) => [...r.errors, ...r.warnings].map((i) => i.message)
const errs = (r) => r.errors.map((i) => i.message)

/* ── 1. the bmgr board must resolve ──────────────────────────────────── */

test('a board that exists in a pack resolves with no board error', () => {
  const root = pack({})
  const r = validateProduct(product({ ci_chips: ['esp32s31'] }), BLOCKS, { boardsDir: root, board: boardFile() })
  assert.deepEqual(errs(r), [])
})

test('a pack reached through a SYMLINK resolves — the assembled boards dir is all symlinks', () => {
  // ZeroCode's image and build-product.sh assemble ONE root whose packs are
  // symlinks to several pack checkouts. A Dirent-based
  // isDirectory() sees none of them; the validator once reported every board
  // on the live host as "not found" this way (2026-09-03).
  const real = pack({})
  const root = mkdtempSync(join(tmpdir(), 'zc-boards-links-'))
  cleanup.push(root)
  symlinkSync(join(real, 'fx_pack'), join(root, 'linked_pack'))
  const r = validateProduct(product({ ci_chips: ['esp32s31'] }), BLOCKS, { boardsDir: root, board: boardFile() })
  assert.deepEqual(errs(r), [])
})

test('a bmgr board in no pack is an error naming it and where packs were looked for', () => {
  const root = pack({})
  const r = validateProduct(product({ ci_chips: ['esp32s31'] }), BLOCKS,
    { boardsDir: root, board: boardFile({ bmgr: 'no_such_board' }) })
  assert.equal(r.errors.length, 1)
  const m = r.errors[0].message
  assert.match(m, /no_such_board/)
  assert.match(m, new RegExp(root.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')))
  assert.match(m, /ZC_BOARDS_DIR/)
})

test('a missing boards root SKIPS the pack checks with a warning, not an error', () => {
  const r = validateProduct(
    product({ ci_chips: ['esp32s31'] }),
    BLOCKS,
    { boardsDir: join(tmpdir(), 'zc-boards-does-not-exist-12345'), board: boardFile() },
  )
  assert.deepEqual(errs(r), [])
  assert.equal(r.warnings.filter((w) => /not checked/.test(w.message)).length, 1)
})

test('a product with NO board file is untouched by any of this', () => {
  // The general case, and the one this whole change is for: a catalog product
  // names no board, so there is no hardware to check it against.
  const root = pack({ peripherals: PERIPHERALS, devices: DEVICES })
  const p = product({ ci_chips: ['esp32c6'] })   // deliberately a different chip
  p.instances[0].cfg = { gpio: 20 }              // deliberately a board-occupied pin
  const r = validateProduct(p, BLOCKS, { boardsDir: root })   // …and NO board file
  assert.deepEqual(errs(r), [])
  assert.deepEqual(msgs(r).filter((m) => /board/.test(m)), [])
})

test('a board file whose bmgr.board is null resolves nothing and checks no pins', () => {
  // Hardware we have no bmgr definition for yet. That is a coverage fact about
  // our packs — it must not turn into an error about the user's board, and it
  // must not silently claim pins we cannot know are occupied.
  const root = pack({ peripherals: PERIPHERALS, devices: DEVICES })
  const p = product({ ci_chips: ['esp32c6'] })
  p.instances[0].cfg = { gpio: 20 }             // occupied on fx_board; unknown here
  const r = validateProduct(p, BLOCKS,
    { boardsDir: root, board: boardFile({ bmgr: null, chip: 'esp32c6', resolved_from: 'fallback' }) })
  assert.deepEqual(errs(r), [])
})

/* ── 2. one chip, three statements, all agreeing ─────────────────────── */
//
// GONE: "a board product must declare ci_chips". A product does not have a
// board any more, so there is no such thing as a board product; ci_chips is
// CI's chip list for a general product and its absence says nothing.

test('a board file with no ci_chips is fine — a general product need not pin one', () => {
  const root = pack({ chip: 'esp32s31' })
  assert.deepEqual(errs(validateProduct(product(), BLOCKS, { boardsDir: root, board: boardFile() })), [])
})

test('ci_chips disagreeing with the board file is an error', () => {
  const root = pack({ chip: 'esp32s31' })
  const r = validateProduct(product({ ci_chips: ['esp32c6'] }), BLOCKS, { boardsDir: root, board: boardFile() })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /ci_chips \[esp32c6\]/)
  assert.match(r.errors[0].message, /esp32s31/)
})

test("the board file's chip must agree with the resolved bmgr board's own", () => {
  // The board file says the user has a C6; the bmgr board it resolved to is an
  // S31 board. One of the two is wrong and the build would prove nothing.
  const root = pack({ chip: 'esp32s31' })
  const r = validateProduct(product(), BLOCKS,
    { boardsDir: root, board: boardFile({ chip: 'esp32c6' }) })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /board file says chip: esp32c6/)
  assert.match(r.errors[0].message, /fx_board/)
  assert.match(r.errors[0].message, /chip: esp32s31/)
})

test('a fallback resolution says so when the module is for the wrong chip', () => {
  const root = pack({ chip: 'esp32s31' })
  const r = validateProduct(product(), BLOCKS,
    { boardsDir: root, board: boardFile({ chip: 'esp32c6', resolved_from: 'fallback' }) })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /fallback board must be the module for the SAME chip/)
})

/* ── 3. pin legality against the BOARD ───────────────────────────────── */

const PERIPHERALS = `
peripherals:
  - name: gpio_status_led
    type: gpio
    config:
      pin: 20
      default_level: 1
  - name: i2c_master
    type: i2c
    config:
      port: 0
      pins:
        sda: 21
        scl: 22
  - name: spi_display
    type: spi
    config:
      spi_bus_config:
        spi_port: SPI2_HOST
        sclk_io_num: 30
        max_transfer_sz: 9600
`

const DEVICES = `
devices:
  - name: display_lcd
    type: display_lcd
    config:
      io_spi_config:
        cs_gpio_num: 31
        dc_gpio_num: -1
      lcd_panel_config:
        reset_gpio_num: 32
`

test('wiring a part onto a pin the board already uses is a CONFLICT error', () => {
  const root = pack({ peripherals: PERIPHERALS, devices: DEVICES })
  const r = validateProduct(
    product({ ci_chips: ['esp32s31'] }), // RELAY.gpio = 20
    BLOCKS,
    { boardsDir: root, board: boardFile() },
  )
  assert.equal(r.errors.length, 1)
  const m = r.errors[0].message
  assert.match(m, /gpio 20/)                     // the pin
  assert.match(m, /drivers\/relay/)              // the product's block
  assert.match(m, /RELAY\.gpio/)
  assert.match(m, /gpio_status_led/)             // what the board uses it for
})

test('a free pin is not a conflict', () => {
  const root = pack({ peripherals: PERIPHERALS, devices: DEVICES })
  const p = product({ ci_chips: ['esp32s31'] })
  p.instances[0].cfg = { gpio: 25 }
  assert.deepEqual(errs(validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })), [])
})

test('pins inside a `pins:` map count as occupied', () => {
  const root = pack({ peripherals: PERIPHERALS, devices: DEVICES })
  const p = product({ ci_chips: ['esp32s31'] })
  p.instances[0].cfg = { gpio: 22 } // i2c scl
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /gpio 22/)
  assert.match(r.errors[0].message, /i2c_master/)
})

test('*_io_num and *_gpio_num under a device count as occupied', () => {
  const root = pack({ peripherals: PERIPHERALS, devices: DEVICES })
  for (const [pin, owner] of [[30, /spi_display/], [31, /display_lcd/], [32, /display_lcd/]]) {
    const p = product({ ci_chips: ['esp32s31'] })
    p.instances[0].cfg = { gpio: pin }
    const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
    assert.equal(r.errors.length, 1, `gpio ${pin} should conflict`)
    assert.match(r.errors[0].message, owner)
  }
})

test('gpio_-prefixed device config keys (the knob) count as occupied', () => {
  // bmgr v0.7.2's knob device: gpio_encoder_a / gpio_encoder_b under config,
  // no *_gpio_num suffix. The M5Dial's encoder pins validated as free until
  // the prefix was recognised.
  const knob = 'devices:\n  - name: knob\n    type: knob\n    sub_type: gpio\n    config:\n      gpio_encoder_a: 40\n      gpio_encoder_b: 41\n'
  const root = pack({ peripherals: 'peripherals: []\n', devices: knob })
  const p = product({ ci_chips: ['esp32s31'] })
  p.instances[0].cfg = { gpio: 41 }
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /knob/)
})

test('a -1 pin is "line not present", not a claim on gpio -1', () => {
  // dc_gpio_num: -1 above. Nothing may be recorded for it; the check must not
  // invent an occupant, and a product can never name -1 anyway.
  const root = pack({ peripherals: PERIPHERALS, devices: DEVICES })
  const p = product({ ci_chips: ['esp32s31'] })
  p.instances[0].cfg = { gpio: -1 }
  assert.deepEqual(errs(validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })), [])
})

test('bus indices (port, spi_port) are not mistaken for pins', () => {
  // `port: 0` on the i2c peripheral would otherwise occupy gpio 0.
  const root = pack({ peripherals: PERIPHERALS, devices: DEVICES })
  const p = product({ ci_chips: ['esp32s31'] })
  p.instances[0].cfg = { gpio: 0 }
  assert.deepEqual(errs(validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })), [])
})

test('several conflicting pins are reported in one error, each named', () => {
  const root = pack({ peripherals: PERIPHERALS, devices: DEVICES })
  const p = product({ ci_chips: ['esp32s31'] })
  p.instances[0].cfg = { gpio: 20, sense_gpio: 21 }
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /gpio 20/)
  assert.match(r.errors[0].message, /gpio 21/)
})

test('the chip-level pin check still runs alongside the board check', () => {
  // gpio 15 is SPICLK on esp32c3 — the existing chip warning must survive.
  const root = pack({ peripherals: PERIPHERALS, devices: DEVICES })
  const p = product({ ci_chips: ['esp32s31'] })
  p.instances[0].cfg = { gpio: 15 }
  const r = validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })
  assert.equal(r.warnings.filter((w) => /chip\/pin compatibility/.test(w.message)).length, 1)
})

/* ── shape robustness ────────────────────────────────────────────────── */

test('a board with no peripherals/devices files resolves and occupies nothing', () => {
  const root = pack({}) // board_info.yaml only
  const p = product({ ci_chips: ['esp32s31'] })
  assert.deepEqual(errs(validateProduct(p, BLOCKS, { boardsDir: root, board: boardFile() })), [])
})

test('a board_info.yaml with no chip: warns rather than failing ci_chips', () => {
  const root = mkdtempSync(join(tmpdir(), 'zc-boards-'))
  cleanup.push(root)
  const dir = join(root, 'fx_pack', 'fx_board')
  mkdirSync(dir, { recursive: true })
  writeFileSync(join(dir, 'board_info.yaml'), 'board: fx_board\n')
  const r = validateProduct(product(), BLOCKS, { boardsDir: root, board: boardFile() })
  assert.deepEqual(errs(r), [])
  assert.equal(r.warnings.filter((w) => /no readable 'chip:'/.test(w.message)).length, 1)
})
