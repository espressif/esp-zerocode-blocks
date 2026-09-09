#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Unit tests for the SHARED BUS rules — `provides_bus:` / `requires_bus:`.
 *
 * What this is evidence for. Eleven driver blocks perform I2C transactions
 * against a port they take as a param and never configure; they assume some
 * other instance did. Undeclared, that assumption was held by CONVENTION only:
 * a product composed with such a sensor and no bus block generates, compiles,
 * links, boots — and is silent on the device, with nothing in any agent report
 * saying why. Every gate we run passes. These tests pin the three answers the
 * validator now gives instead:
 *
 *   1. no provider on that port                → error naming block, port, fix
 *   2. a provider, but LATER in instances:     → error too. Init order is
 *      product.yml order (app_driver_init calls each instance's driver_init in
 *      sequence), so a bus configured after its first transaction is exactly
 *      the same bug.
 *   3. a `board:` product whose BOARD carries that bus → satisfied, no
 *      instance needed. Requiring one would configure a bus twice.
 *
 * Section 5 pins the inverse of a guard that used to live here: while the
 * consumers spoke the legacy driver/i2c.h a board-owned bus under one of them
 * was refused, and now that every I2C block resolves its bus through
 * i2c_master_get_bus_handle(port) it is ordinary.
 *
 * Standalone: node's built-in runner against engine/dist, fixtures in a temp
 * dir, no monorepo and no board-pack checkout.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { mkdtempSync, mkdirSync, writeFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const { validateProduct, validateBlock } = await import(join(ROOT, 'engine/dist/index.js'))

/* ── fixtures ────────────────────────────────────────────────────────── */

/** A bus provider, a bus user, an SPI pair (the mechanism is not I2C-only),
 *  and a block on no bus at all. */
const BLOCKS = new Map([
  ['peripherals/fx_i2c_bus', {
    id: 'peripherals/fx_i2c_bus',
    kind: 'peripheral',
    description: 'configures an i2c bus',
    provides_bus: { type: 'i2c', port_param: 'port' },
    params: { port: { type: 'int', default: 0 }, sda_gpio: { type: 'int' }, scl_gpio: { type: 'int' } },
  }],
  ['drivers/fx_sensor', {
    id: 'drivers/fx_sensor',
    kind: 'driver',
    description: 'talks on an i2c bus it does not configure',
    requires_bus: { type: 'i2c', port_param: 'i2c_port' },
    params: { i2c_port: { type: 'int', default: 0 }, i2c_address: { type: 'int', default: 0x44 } },
  }],
  ['peripherals/fx_spi_bus', {
    id: 'peripherals/fx_spi_bus',
    kind: 'peripheral',
    description: 'configures an spi host',
    provides_bus: { type: 'spi', port_param: 'host' },
    params: { host: { type: 'int', default: 1 } },
  }],
  ['drivers/fx_spi_part', {
    id: 'drivers/fx_spi_part',
    kind: 'driver',
    description: 'talks on an spi host it does not configure',
    requires_bus: { type: 'spi', port_param: 'spi_host' },
    params: { spi_host: { type: 'int', default: 1 } },
  }],
  ['drivers/fx_plain', {
    id: 'drivers/fx_plain',
    kind: 'driver',
    description: 'no bus at all',
    params: { gpio: { type: 'int', default: 3 } },
  }],
])

const product = (instances, over = {}) => ({
  id: 'fx-product', name: 'fx', description: 'fx', keywords: [], frameworks: [], instances, ...over,
})

const bus = (cfg = {}) => ({ block: 'peripherals/fx_i2c_bus', prefix: 'BUS', cfg: { sda_gpio: 4, scl_gpio: 5, ...cfg } })
const sensor = (prefix = 'S', cfg = {}) => ({ block: 'drivers/fx_sensor', prefix, cfg })

const errs = (p, opts) => validateProduct(p, BLOCKS, opts).errors.map((e) => e.message)
const busErrs = (p, opts) => errs(p, opts).filter((m) => /talks on/.test(m))

/** A board pack whose board optionally carries its own i2c peripheral. */
const cleanup = []
function pack({ peripherals } = {}) {
  const root = mkdtempSync(join(tmpdir(), 'zc-bus-boards-'))
  cleanup.push(root)
  const dir = join(root, 'fx_pack', 'fx_board')
  mkdirSync(dir, { recursive: true })
  writeFileSync(join(dir, 'board_info.yaml'), 'board: fx_board\nchip: esp32c6\n')
  if (peripherals !== undefined) writeFileSync(join(dir, 'board_peripherals.yaml'), peripherals)
  return root
}
test.after(() => { for (const r of cleanup) rmSync(r, { recursive: true, force: true }) })

/** The board file a product instance on the fixture board carries. The board
 *  is a separate input, never a product key. */
const BOARD = { schema: 'zc-board/1', selected: 'fx-devkit', chip: 'esp32c6', bmgr: { board: 'fx_board', resolved_from: 'exact' } }

const BOARD_I2C0 = `peripherals:
  - name: i2c_master
    type: i2c
    role: master
    config:
      port: 0
      pins: { sda: 8, scl: 9 }
`

/* ── 1. the missing bus ──────────────────────────────────────────────── */

test('a sensor with no bus in the product is an ERROR that names block, port and fix', () => {
  const messages = busErrs(product([sensor()]))
  assert.equal(messages.length, 1)
  const m = messages[0]
  assert.match(m, /drivers\/fx_sensor \(prefix=S\)/)          // which instance
  assert.match(m, /i2c port 0/)                                 // which bus
  assert.match(m, /the device is silent/)                       // what happens today
  assert.match(m, /peripherals\/fx_i2c_bus \(cfg\.port: 0\)/)  // what to add, and how
  assert.match(m, /EARLIER in instances:/)
})

test('the provider named in the fix is DERIVED from the catalog, not hardcoded', () => {
  // The spi pair proves the rule is about buses, not about I2C: nothing in the
  // engine mentions spi, and the fix line still names the right provider.
  const m = busErrs(product([{ block: 'drivers/fx_spi_part', prefix: 'P', cfg: {} }]))
  assert.equal(m.length, 1)
  assert.match(m[0], /spi host 1|spi port 1/)
  assert.match(m[0], /peripherals\/fx_spi_bus \(cfg\.host: 1\)/)
  assert.ok(!/fx_i2c_bus/.test(m[0]), 'an i2c provider is not an answer for an spi requirement')
})

test('a bus on the WRONG port does not satisfy the requirement', () => {
  const m = busErrs(product([bus({ port: 0 }), sensor('S', { i2c_port: 1 })]))
  assert.equal(m.length, 1)
  assert.match(m[0], /i2c port 1/)
})

test('a block with no bus declarations is never asked about one', () => {
  assert.deepEqual(busErrs(product([{ block: 'drivers/fx_plain', prefix: 'P', cfg: {} }])), [])
})

/* ── 2. order is the bug, not a nit ──────────────────────────────────── */

test('a provider EARLIER in instances: satisfies the requirement', () => {
  assert.deepEqual(busErrs(product([bus(), sensor()])), [])
})

test('the SAME two instances in the other order are an error', () => {
  // This is the pair that matters: identical products, identical pins, and one
  // of them talks to a bus that does not exist yet.
  const m = busErrs(product([sensor(), bus()]))
  assert.equal(m.length, 1)
  assert.match(m[0], /comes LATER in instances: \(position 2 vs 1\)/)
  assert.match(m[0], /peripherals\/fx_i2c_bus \(prefix=BUS\)/)
  assert.match(m[0], /Init order is product\.yml order/)
})

test('one bus satisfies every sensor on it, and each sensor is judged on its own position', () => {
  const p = product([sensor('A'), bus(), sensor('B')])
  const m = busErrs(p)
  assert.equal(m.length, 1, 'B is fine; only A is above the bus')
  assert.match(m[0], /prefix=A/)
})

/* ── 3. a board can be the provider ──────────────────────────────────── */

test("a product on a board is satisfied by the BOARD's own i2c peripheral", () => {
  const p = product([sensor()], { ci_chips: ['esp32c6'] })
  assert.deepEqual(busErrs(p, { boardsDir: pack({ peripherals: BOARD_I2C0 }), board: BOARD }), [])
})

test('a board WITHOUT that bus does not satisfy it, and the error says a board could', () => {
  const p = product([sensor()], { ci_chips: ['esp32c6'] })
  const m = busErrs(p, { boardsDir: pack({ peripherals: 'peripherals: []\n' }), board: BOARD })
  assert.equal(m.length, 1)
  assert.match(m[0], /or use a board whose own i2c peripheral is on port 0/)
})

test("the board's bus must be on the SAME port", () => {
  const p = product([sensor('S', { i2c_port: 1 })], { ci_chips: ['esp32c6'] })
  const m = busErrs(p, { boardsDir: pack({ peripherals: BOARD_I2C0 }), board: BOARD })
  assert.equal(m.length, 1, 'the board carries i2c0; the sensor is on i2c1')
})

test('an unreadable board pack degrades to a WARNING, never a false error', () => {
  // Same stance as the rest of the board checks: board packs are host content
  // like ESP-IDF, and a machine without them must still be able to validate.
  const p = product([sensor()], { ci_chips: ['esp32c6'] })
  const r = validateProduct(p, BLOCKS, { boardsDir: join(tmpdir(), 'zc-no-such-boards-dir'), board: BOARD })
  assert.deepEqual(r.errors.filter((e) => /talks on/.test(e.message)), [])
  assert.ok(r.warnings.some((w) => /needs i2c port 0 configured/.test(w.message)))
})

/* ── 4. authoring mistakes in the declaration itself ─────────────────── */

test('a bus declaration naming a param the block does not have is a BLOCK error', () => {
  // Such a declaration resolves to an empty port and quietly matches nothing —
  // protection that looks present and is not.
  const r = validateBlock({
    id: 'drivers/fx_typo', kind: 'driver', description: 'typo',
    requires_bus: { type: 'i2c', port_param: 'i2cport' },
    params: { i2c_port: { type: 'int' } },
  })
  assert.equal(r.errors.length, 1)
  assert.match(r.errors[0].message, /requires_bus names port_param 'i2cport', which the block does not declare/)
})

test('a declaration missing its type or port_param is a BLOCK error', () => {
  const r = validateBlock({
    id: 'drivers/fx_bad', kind: 'driver', description: 'bad',
    provides_bus: { type: 'i2c' },
    params: {},
  })
  assert.match(r.errors.map((e) => e.message).join('\n'), /provides_bus entry needs a 'port_param'/)
})

test('a block may declare several buses at once', () => {
  // An SPI panel with an I2C touch controller needs both; the list form is the
  // same declaration written twice.
  const blocks = new Map([...BLOCKS, ['drivers/fx_panel', {
    id: 'drivers/fx_panel', kind: 'driver', description: 'panel + touch',
    requires_bus: [{ type: 'spi', port_param: 'spi_host' }, { type: 'i2c', port_param: 'i2c_port' }],
    params: { spi_host: { type: 'int', default: 1 }, i2c_port: { type: 'int', default: 0 } },
  }]])
  const p = product([{ block: 'drivers/fx_panel', prefix: 'PANEL', cfg: {} }])
  const m = validateProduct(p, blocks).errors.map((e) => e.message).filter((x) => /talks on/.test(x))
  assert.equal(m.length, 2, 'both buses are checked')
  assert.ok(m.some((x) => /spi/.test(x)) && m.some((x) => /i2c/.test(x)))
})

/* ── 5. a board-owned bus is an ORDINARY bus ─────────────────────────── */

test('a provider that hands its bus to the BOARD MANAGER still satisfies a consumer', () => {
  // The inverse of a guard that used to live here. While the consumers spoke
  // the legacy driver/i2c.h, a provider whose bmgr mapping handed the port to
  // esp_board_manager (which creates it with driver/i2c_master.h) made the
  // product silent on the device, and this pair was REFUSED. Every I2C block
  // now resolves its bus with i2c_master_get_bus_handle(port), so who created
  // the bus is invisible to a consumer and the pair is fine.
  //
  // Break-on-purpose: reinstate the old `if (bmgrBoard && handedOver)` push in
  // validator.ts and this test fails with the "hands that bus to the board
  // manager" message.
  const blocks = new Map([...BLOCKS, ['peripherals/fx_i2c_bus', {
    ...BLOCKS.get('peripherals/fx_i2c_bus'),
    bmgr: { init: 'board_manager', replaces_slots: ['driver_init'], peripherals: [{ name: 'i2c0', type: 'i2c' }] },
  }]])
  const instances = [bus({ port: 0 }), sensor()]
  const boardsDir = pack({ peripherals: 'peripherals: []\n' })

  // Boardless, as before.
  assert.deepEqual(
    validateProduct(product(instances), blocks).errors.filter((e) => /talks on/.test(e.message)),
    [],
  )
  // The SAME instances with a board: also fine now.
  assert.deepEqual(
    validateProduct(product(instances, { ci_chips: ['esp32c6'] }), blocks, { boardsDir, board: BOARD })
      .errors.filter((e) => /talks on/.test(e.message)),
    [],
  )
})

test('order still matters on a board — the board manager does not excuse a late provider', () => {
  // The ordering rule is about init sequence, not about which driver API is in
  // play, so removing the legacy guard must not have removed it. A provider
  // whose bmgr mapping is present but which sits BELOW its consumer is still
  // an error, because on a boardless build of the same product it is one.
  const blocks = new Map([...BLOCKS, ['peripherals/fx_i2c_bus', {
    ...BLOCKS.get('peripherals/fx_i2c_bus'),
    bmgr: { init: 'board_manager', replaces_slots: ['driver_init'], peripherals: [{ name: 'i2c0', type: 'i2c' }] },
  }]])
  const m = validateProduct(product([sensor(), bus({ port: 0 })]), blocks)
    .errors.map((e) => e.message).filter((x) => /talks on/.test(x))
  assert.equal(m.length, 1)
  assert.match(m[0], /comes LATER in instances:/)
})
