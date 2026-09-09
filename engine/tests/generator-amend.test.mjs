#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Unit tests for the generated AMEND overlay — a `board:` product's
 * externally wired parts, turned into the board_amend/ directory
 * `idf.py bmgr -a` applies on top of the board definition.
 *
 * What each group is evidence for:
 *   - the amend exists, has bmgr's manifest shape, and carries TYPED values
 *     (pin: 4, not pin: "4" — bmgr's schema is typed and a string pin is a
 *     silent parse failure);
 *   - a boardless product is untouched: no directory, and the slots a mapping
 *     would replace are still rendered;
 *   - a mapping's guards are a HARD gate. The active-low relay case is the
 *     reason the whole guard mechanism exists (periph_gpio drives the pin
 *     before it writes default_level), and a guard that could be skipped
 *     silently would be no protection at all;
 *   - authoring mistakes that are invisible on a boardless product (a guard on
 *     an undeclared param, two instances claiming one bmgr name) fail loudly.
 *
 * Standalone, like the other engine tests: node's built-in runner against
 * engine/dist with a base_firmware + block tree built in a temp dir.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, existsSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parse as parseYaml } from 'yaml'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const { generate, buildBoardAmend, AMEND_DIR } = await import(join(ROOT, 'engine/dist/index.js'))

/* ── fixtures ────────────────────────────────────────────────────────── */

const APP_MAIN = `#include "app_driver.h"
{{main_includes}}

extern "C" void app_main(void)
{
{{board_init}}
    ESP_ERROR_CHECK(app_driver_init());
{{main_init}}
}
`

/** A relay-shaped block: an active-level-guarded gpio mapping that also takes
 *  the block's own driver_init over. Mirrors code_blocks/drivers/relay. */
const RELAY_BLOCK = `id: drivers/fx_relay
kind: driver
description: fixture relay
params:
  gpio:
    type: int
    required: true
  active_level:
    type: int
    default: 1
bmgr:
  init: board_manager
  replaces_slots: [driver_init]
  requires:
    - cfg: active_level
      equals: 1
      reason: >-
        periph_gpio drives the pin before it writes default_level, so an
        active-LOW load is energized at every boot.
  peripherals:
    - name: gpio_{{prefix_lc}}_relay
      type: gpio
      role: io
      version: default
      config:
        pin: '{{cfg.gpio}}'
        mode: GPIO_MODE_OUTPUT
        default_level: 0
slots:
  driver_init: |
    /* BLOCK-OWNED INIT for {{prefix_lc}} on gpio {{cfg.gpio}} */
  driver_apply_cases: |
    /* apply {{prefix_lc}} */
`

/** A block with no bmgr: at all — the untouched majority. */
const PLAIN_BLOCK = `id: drivers/fx_plain
kind: driver
description: fixture plain driver
params:
  gpio:
    type: int
    default: 3
slots:
  driver_init: |
    /* plain init on gpio {{cfg.gpio}} */
`

/** A block whose mapping emits a DEVICE as well as a peripheral. */
const BUTTON_BLOCK = `id: drivers/fx_button
kind: driver
description: fixture button
params:
  gpio:
    type: int
    required: true
bmgr:
  init: board_manager
  peripherals:
    - name: gpio_{{prefix_lc}}_btn
      type: gpio
      role: io
      config:
        pin: '{{cfg.gpio}}'
        mode: GPIO_MODE_INPUT
  devices:
    - name: '{{prefix_lc}}_button'
      type: button
      sub_type: gpio
      version: default
      peripherals:
        - gpio_name: gpio_{{prefix_lc}}_btn
`

function scaffold(t, extraBlocks = {}) {
  const root = mkdtempSync(join(tmpdir(), 'zc-amend-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))

  const base = join(root, 'base_firmware')
  mkdirSync(join(base, 'main'), { recursive: true })
  writeFileSync(join(base, 'main/app_main.cpp'), APP_MAIN)
  writeFileSync(join(base, 'main/idf_component.yml'), 'dependencies: {}\n')
  writeFileSync(join(base, 'partitions.csv'), 'factory, app, factory, 0x10000, 0x100000,\n')
  writeFileSync(join(base, 'sdkconfig.defaults'), '# base\n')

  const templates = join(root, 'templates')
  const blocks = {
    'drivers/fx_relay': RELAY_BLOCK,
    'drivers/fx_plain': PLAIN_BLOCK,
    'drivers/fx_button': BUTTON_BLOCK,
    ...extraBlocks,
  }
  for (const [id, body] of Object.entries(blocks)) {
    const dir = join(templates, 'code_blocks', id)
    mkdirSync(dir, { recursive: true })
    writeFileSync(join(dir, 'block.yml'), body)
  }
  mkdirSync(join(templates, 'product_configurations'), { recursive: true })

  const boards = join(root, 'boards', 'fx_pack', 'fx_board')
  mkdirSync(boards, { recursive: true })
  writeFileSync(join(boards, 'board_info.yaml'), 'board: fx_board\nchip: esp32c6\n')

  return {
    paths: { templatesDir: templates, baseFirmwareDir: base, boardsDir: join(root, 'boards') },
    outDir: join(root, 'out'),
  }
}

const relayInstance = (over = {}) => ({
  block: 'drivers/fx_relay', prefix: 'APP', cfg: { gpio: 10, ...over },
})

const product = (over = {}) => ({
  id: 'fx-product', name: 'fx', description: 'fx',
  frameworks: [], instances: [relayInstance()], ...over,
})

/** The board file a product INSTANCE on the fixture board would carry. The
 *  board is not a product key any more (see engine/src/types.ts): it is a
 *  separate document handed to generate() beside the product. */
const BOARD = {
  schema: 'zc-board/1',
  selected: 'fx-devkit',
  chip: 'esp32c6',
  bmgr: { board: 'fx_board', resolved_from: 'exact' },
}

/** generate() input for this product running ON the fixture board. */
const onBoard = (over = {}) => ({ product: product(over), board: BOARD, chip: BOARD.chip })

const amendFiles = (outDir) => ({
  manifest: join(outDir, AMEND_DIR, 'board_amend.yaml'),
  fragment: join(outDir, AMEND_DIR, 'zerocode_parts.yaml'),
})

/* ── 1. the amend a board product gets ───────────────────────────────── */

test('a board product writes the manifest + fragment bmgr -a expects', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, { ...onBoard(), outDir })
  const { manifest, fragment } = amendFiles(outDir)

  // bmgr only reads files NAMED in apply:, so the manifest has to point at the
  // fragment by its real filename or the whole overlay is silently ignored.
  const m = parseYaml(readFileSync(manifest, 'utf-8'))
  assert.equal(m.version, '1.0')
  assert.deepEqual(m.apply, ['zerocode_parts.yaml'])
  assert.ok(existsSync(fragment), 'the file apply: names actually exists')

  const f = parseYaml(readFileSync(fragment, 'utf-8'))
  assert.equal(f.peripherals.length, 1)
  const p = f.peripherals[0]
  assert.equal(p.name, 'gpio_app_relay', 'the name is templated per instance')
  assert.equal(p.type, 'gpio')
  assert.equal(p.config.mode, 'GPIO_MODE_OUTPUT')
  // TYPE, not just value: bmgr's schema wants an int, and `pin: "10"` is a
  // different thing that fails deep inside its generator.
  assert.equal(p.config.pin, 10)
  assert.equal(typeof p.config.pin, 'number')
})

test('devices come through as well as peripherals, cross-referenced by name', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, {
    ...onBoard({ instances: [{ block: 'drivers/fx_button', prefix: 'BOOT', cfg: { gpio: 7 } }] }),
    outDir,
  })
  const f = parseYaml(readFileSync(amendFiles(outDir).fragment, 'utf-8'))
  assert.equal(f.peripherals[0].name, 'gpio_boot_btn')
  assert.equal(f.devices[0].name, 'boot_button')
  // The device points at the peripheral the same mapping emitted — both names
  // went through the same substitution, so they cannot drift apart.
  assert.equal(f.devices[0].peripherals[0].gpio_name, f.peripherals[0].name)
})

test('two instances of one block get one peripheral each, named apart', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, {
    ...onBoard({
      instances: [relayInstance(), { block: 'drivers/fx_relay', prefix: 'AUX', cfg: { gpio: 11 } }],
    }),
    outDir,
  })
  const f = parseYaml(readFileSync(amendFiles(outDir).fragment, 'utf-8'))
  assert.deepEqual(f.peripherals.map((p) => p.name), ['gpio_app_relay', 'gpio_aux_relay'])
  assert.deepEqual(f.peripherals.map((p) => p.config.pin), [10, 11])
})

/* ── 2. no board, or nothing mapped: no amend at all ─────────────────── */

test('a product with no board: gets NO amend directory', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, { product: product(), chip: 'esp32c6', outDir })
  assert.equal(existsSync(join(outDir, AMEND_DIR)), false)
})

test('a board product whose blocks declare no mapping gets NO amend directory', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, {
    ...onBoard({ instances: [{ block: 'drivers/fx_plain', prefix: 'P', cfg: {} }] }),
    outDir,
  })
  // The build step keys `-a` off the manifest's existence, so "nothing to
  // amend" has to mean no directory — not an empty one.
  assert.equal(existsSync(join(outDir, AMEND_DIR)), false)
})

/* ── 3. one pin, one owner ───────────────────────────────────────────── */

test('the block keeps its own init when there is no board', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, { product: product(), chip: 'esp32c6', outDir })
  const driver = readFileSync(join(outDir, 'components/app_driver/app_driver.cpp'), 'utf-8')
  assert.ok(driver.includes('BLOCK-OWNED INIT for app on gpio 10'))
})

test('a replaced slot is NOT rendered when the board manager owns the line', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, { ...onBoard(), outDir })
  const driver = readFileSync(join(outDir, 'components/app_driver/app_driver.cpp'), 'utf-8')
  assert.ok(!driver.includes('BLOCK-OWNED INIT'), 'bmgr configures the pin; the block must not also')
  // Only the NAMED slot goes. Everything else the block contributes stays, or
  // "hand the init over" would quietly mean "delete the block".
  assert.ok(driver.includes('/* apply app */'))
})

test('a block with no mapping keeps its init even on a board product', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, {
    ...onBoard({ instances: [relayInstance(), { block: 'drivers/fx_plain', prefix: 'P', cfg: {} }] }),
    outDir,
  })
  const driver = readFileSync(join(outDir, 'components/app_driver/app_driver.cpp'), 'utf-8')
  assert.ok(driver.includes('plain init on gpio 3'))
  assert.ok(!driver.includes('BLOCK-OWNED INIT'))
})

/* ── 4. guards are a hard gate ───────────────────────────────────────── */

test('an ACTIVE-LOW output is refused, loudly, with the block author\'s reason', async (t) => {
  const { paths, outDir } = scaffold(t)
  await assert.rejects(
    generate(paths, { ...onBoard({ instances: [relayInstance({ active_level: 0 })] }), outDir }),
    (e) => {
      assert.match(e.message, /drivers\/fx_relay \(prefix=APP\)/)   // which instance
      assert.match(e.message, /cfg\.active_level is 0/)              // what it said
      assert.match(e.message, /only declared valid for 1/)           // what was allowed
      assert.match(e.message, /before it writes default_level/)      // WHY — the author's words
      assert.match(e.message, /clear bmgr\.board \('fx_board'\)/)     // what to do
      return true
    },
  )
})

test('the guard passes on the value it vouches for, including as a string', async (t) => {
  const { paths, outDir } = scaffold(t)
  // YAML types vary by author: `active_level: "1"` is the same answer.
  await generate(paths, { ...onBoard({ instances: [relayInstance({ active_level: '1' })] }), outDir })
  assert.ok(existsSync(amendFiles(outDir).fragment))
})

test('an unsafe instance is refused rather than silently left out of the amend', async (t) => {
  const { paths, outDir } = scaffold(t)
  // Two instances, one safe: the safe one must NOT be quietly generated on its
  // own. A partial amend looks exactly like a correct one.
  await assert.rejects(
    generate(paths, {
      ...onBoard({
        instances: [relayInstance(), { block: 'drivers/fx_relay', prefix: 'AUX', cfg: { gpio: 11, active_level: 0 } }],
      }),
      outDir,
    }),
    /prefix=AUX/,
  )
  assert.equal(existsSync(join(outDir, AMEND_DIR)), false, 'nothing is left behind by a refused generation')
})

test('a guard is only skipped when the product has no board at all', async (t) => {
  const { paths, outDir } = scaffold(t)
  // Same active-low instance, no board: generation is a non-event, because
  // the block's own (safe) init is what runs.
  await generate(paths, { product: product({ instances: [relayInstance({ active_level: 0 })] }), chip: 'esp32c6', outDir })
  const driver = readFileSync(join(outDir, 'components/app_driver/app_driver.cpp'), 'utf-8')
  assert.ok(driver.includes('BLOCK-OWNED INIT'))
})

/* ── 5. authoring mistakes that are otherwise invisible ──────────────── */

test('a guard on a param the block does not declare is an authoring error', async (t) => {
  const { paths, outDir } = scaffold(t, {
    'drivers/fx_typo': `id: drivers/fx_typo
kind: driver
description: guard names a param that does not exist
params:
  gpio: { type: int }
bmgr:
  init: board_manager
  requires:
    - cfg: activelevel
      equals: 1
      reason: typo'd guard
  peripherals:
    - name: gpio_{{prefix_lc}}
      type: gpio
      config: { pin: '{{cfg.gpio}}' }
`,
  })
  // Such a guard can never fail, so it is protection that isn't there.
  await assert.rejects(
    generate(paths, { ...onBoard({ instances: [{ block: 'drivers/fx_typo', prefix: 'T', cfg: { gpio: 5 } }] }), outDir }),
    /guards cfg\.activelevel, which the block does not declare/,
  )
})

test('two instances claiming ONE bmgr name is an error, not a silent merge', async (t) => {
  const { paths, outDir } = scaffold(t, {
    'drivers/fx_fixed': `id: drivers/fx_fixed
kind: driver
description: mapping with a name that does not vary per instance
params:
  gpio: { type: int }
bmgr:
  init: board_manager
  peripherals:
    - name: gpio_fixed
      type: gpio
      config: { pin: '{{cfg.gpio}}' }
`,
  })
  // bmgr merges same-named entries FIELD BY FIELD, so the second instance's
  // pin would overwrite the first's and one relay would go unclaimed.
  await assert.rejects(
    generate(paths, {
      ...onBoard({
        instances: [
          { block: 'drivers/fx_fixed', prefix: 'A', cfg: { gpio: 5 } },
          { block: 'drivers/fx_fixed', prefix: 'B', cfg: { gpio: 6 } },
        ],
      }),
      outDir,
    }),
    (e) => {
      assert.match(e.message, /both emit the bmgr peripheral 'gpio_fixed'/)
      assert.match(e.message, /prefix=A/)
      assert.match(e.message, /prefix=B/)
      assert.match(e.message, /\{\{prefix_lc\}\}/)   // says how to fix it
      return true
    },
  )
})

test('an unimplemented bmgr.init word is refused rather than assumed', async (t) => {
  const { paths, outDir } = scaffold(t, {
    'drivers/fx_future': `id: drivers/fx_future
kind: driver
description: init mode that does not exist yet
params:
  gpio: { type: int }
bmgr:
  init: block
  peripherals:
    - name: gpio_{{prefix_lc}}
      type: gpio
      config: { pin: '{{cfg.gpio}}' }
`,
  })
  await assert.rejects(
    generate(paths, { ...onBoard({ instances: [{ block: 'drivers/fx_future', prefix: 'F', cfg: { gpio: 5 } }] }), outDir }),
    /bmgr\.init must be 'board_manager'/,
  )
})

/* ── 6. the pure builder, directly ───────────────────────────────────── */

test('buildBoardAmend returns null when there is no board', () => {
  const rendered = [{ block: { id: 'drivers/x', bmgr: { init: 'board_manager', peripherals: [{ name: 'n' }] } }, prefix: 'X', cfg: {}, slots: {} }]
  assert.equal(buildBoardAmend(product(), rendered, null), null)
  assert.notEqual(buildBoardAmend(product(), rendered, 'fx_board'), null)
})

/* ── 7. SHARED resources: one bus, however many parts hang off it ─────
 *
 *  The per-instance mapping above is right for a relay (one instance, one
 *  line) and wrong for a bus: two sensors on one I2C port would emit two `i2c`
 *  peripherals, and bmgr merges same-named entries FIELD BY FIELD, so which
 *  SDA pin the bus really runs on would be decided by instance order.
 *
 *  A fragment carrying `shared_key:` is emitted ONCE per distinct RESOLVED
 *  key. Two instances resolving one key to different YAML is the "one bus, two
 *  opinions" case and is an ERROR — the one thing that must never be resolved
 *  silently. Nothing about it is I2C-specific.                              */

/** A bus-shaped block: its fragment is the BUS, not this instance's view of it. */
const BUS_BLOCK = `id: peripherals/fx_i2c_bus
kind: peripheral
description: fixture shared i2c bus
params:
  port:
    type: int
    default: 0
  sda_gpio:
    type: int
    required: true
  scl_gpio:
    type: int
    required: true
bmgr:
  init: board_manager
  replaces_slots: [driver_init]
  peripherals:
    - shared_key: 'i2c-{{cfg.port}}'
      name: 'i2c_zc_{{cfg.port}}'
      type: i2c
      role: master
      config:
        port: '{{cfg.port}}'
        pins:
          sda: '{{cfg.sda_gpio}}'
          scl: '{{cfg.scl_gpio}}'
slots:
  driver_init: |
    /* BLOCK-OWNED BUS INIT on port {{cfg.port}} */
`

const busInstance = (prefix, cfg) => ({ block: 'peripherals/fx_i2c_bus', prefix, cfg })

test('two instances on ONE bus emit exactly one peripheral', async (t) => {
  const { paths, outDir } = scaffold(t, { 'peripherals/fx_i2c_bus': BUS_BLOCK })
  await generate(paths, {
    ...onBoard({
      instances: [
        busInstance('A', { port: 0, sda_gpio: 4, scl_gpio: 5 }),
        busInstance('B', { port: 0, sda_gpio: 4, scl_gpio: 5 }),
      ],
    }),
    outDir,
  })
  const f = parseYaml(readFileSync(amendFiles(outDir).fragment, 'utf-8'))
  assert.equal(f.peripherals.length, 1, 'one bus, one peripheral')
  assert.equal(f.peripherals[0].name, 'i2c_zc_0')
  assert.equal(f.peripherals[0].config.pins.sda, 4)
  // shared_key is OURS — bmgr's schema has no such field and would reject it.
  assert.ok(!('shared_key' in f.peripherals[0]), 'the marker never reaches the emitted YAML')
})

test('two DIFFERENT buses are two peripherals — the key is what makes them one', async (t) => {
  const { paths, outDir } = scaffold(t, { 'peripherals/fx_i2c_bus': BUS_BLOCK })
  await generate(paths, {
    ...onBoard({
      instances: [
        busInstance('A', { port: 0, sda_gpio: 4, scl_gpio: 5 }),
        busInstance('B', { port: 1, sda_gpio: 6, scl_gpio: 7 }),
      ],
    }),
    outDir,
  })
  const f = parseYaml(readFileSync(amendFiles(outDir).fragment, 'utf-8'))
  assert.deepEqual(f.peripherals.map((p) => p.name), ['i2c_zc_0', 'i2c_zc_1'])
  assert.deepEqual(f.peripherals.map((p) => p.config.port), [0, 1])
})

test('ONE BUS, TWO OPINIONS is an error naming both instances and the field', async (t) => {
  const { paths, outDir } = scaffold(t, { 'peripherals/fx_i2c_bus': BUS_BLOCK })
  await assert.rejects(
    generate(paths, {
      ...onBoard({
        instances: [
          busInstance('A', { port: 0, sda_gpio: 4, scl_gpio: 5 }),
          busInstance('B', { port: 0, sda_gpio: 8, scl_gpio: 5 }),   // same port, other SDA
        ],
      }),
      outDir,
    }),
    (e) => {
      assert.match(e.message, /shared peripheral 'i2c-0'/)            // which resource
      assert.match(e.message, /prefix=A/)                              // both instances,
      assert.match(e.message, /prefix=B/)                              //   not just the loser
      assert.match(e.message, /config\.pins\.sda: 4 vs 8/)             // WHICH field, both values
      assert.ok(!/config\.pins\.scl/.test(e.message), 'fields that agree are not listed')
      assert.match(e.message, /silently win/)                          // why it is not merged
      return true
    },
  )
  assert.equal(existsSync(join(outDir, AMEND_DIR)), false, 'nothing is left behind')
})

test('a shared fragment still replaces the block-owned init exactly once', async (t) => {
  const { paths, outDir } = scaffold(t, { 'peripherals/fx_i2c_bus': BUS_BLOCK })
  await generate(paths, {
    ...onBoard({
      instances: [busInstance('A', { port: 0, sda_gpio: 4, scl_gpio: 5 }), busInstance('B', { port: 0, sda_gpio: 4, scl_gpio: 5 })],
    }),
    outDir,
  })
  const driver = readFileSync(join(outDir, 'components/app_driver/app_driver.cpp'), 'utf-8')
  assert.ok(!driver.includes('BLOCK-OWNED BUS INIT'), 'the board manager owns the bus, so the block must not configure it')
})

test('a shared key that resolves to nothing usable is refused, not stringified', async (t) => {
  const { paths, outDir } = scaffold(t, {
    'peripherals/fx_bad_key': `id: peripherals/fx_bad_key
kind: peripheral
description: shared_key that is not a scalar
params:
  ports:
    type: int
bmgr:
  init: board_manager
  peripherals:
    - shared_key: '{{cfg.ports}}'
      name: fixed
      type: i2c
`,
  })
  await assert.rejects(
    generate(paths, {
      ...onBoard({ instances: [{ block: 'peripherals/fx_bad_key', prefix: 'K', cfg: { ports: [0, 1] } }] }),
      outDir,
    }),
    /shared_key that resolved to \[0,1\]/,
  )
})

test('a shared fragment is inert on a boardless product', async (t) => {
  const { paths, outDir } = scaffold(t, { 'peripherals/fx_i2c_bus': BUS_BLOCK })
  await generate(paths, {
    product: product({ instances: [busInstance('A', { port: 0, sda_gpio: 4, scl_gpio: 5 }), busInstance('B', { port: 0, sda_gpio: 8, scl_gpio: 5 })] }),
    chip: 'esp32c6',
    outDir,
  })
  // Even the DISAGREEING pair above: with no board there is no amend, and the
  // blocks keep their own init, exactly as before this mechanism existed.
  assert.equal(existsSync(join(outDir, AMEND_DIR)), false)
  const driver = readFileSync(join(outDir, 'components/app_driver/app_driver.cpp'), 'utf-8')
  assert.ok(driver.includes('BLOCK-OWNED BUS INIT'))
})
