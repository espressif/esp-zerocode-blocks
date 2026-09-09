#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Two block behaviours the 2026-09-05 code_writing batch showed every arm
 * inheriting, guarded here so they cannot come back:
 *
 *   - `drivers/gpio_input_sensor` DEBOUNCES (N consecutive polls) and
 *     RE-ASSERTS its reading when the bus disagrees. Before this it published
 *     on the first differing sample — contact bounce became a burst of edges —
 *     and an external write to its param (a console override) stuck until the
 *     hardware happened to toggle.
 *   - `behaviors/wifi_disconnect_recovery` does NOT reboot unless a product
 *     opts in. The baseline used to carry `reboot_after_seconds: 600`, so
 *     every product rebooted itself ten minutes into a Wi-Fi outage — a pump
 *     controller whose spec said "Wi-Fi down → unaffected" included.
 *
 * Generates over the REAL assembled catalog (like scripts/validate.mjs), so a
 * baseline or block edit that undoes either shows up as a failing test here,
 * not in a product a week later.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, readdirSync, rmSync, existsSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

function assembled() {
  const out = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(out, 'code_blocks'), '--products-out', join(out, 'product_configurations')], { stdio: 'ignore' })
  return out
}

async function generateProduct(templatesDir, id) {
  const paths = { templatesDir, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const products = await eng.listAllProducts(paths)
  const entry = products.find((p) => p.product.id === id || p.id === id)
  assert.ok(entry, `product ${id} in the catalog`)
  const outDir = mkdtempSync(join(tmpdir(), `zc-out-${id}-`))
  // The tree is single-chip; any buildable chip does for a slot-content check.
  await eng.generate(paths, { product: entry.product, board: null, outDir, chip: 'esp32c3' })
  return outDir
}

function readAll(dir, rel) {
  const p = join(dir, rel)
  assert.ok(existsSync(p), `${rel} generated`)
  return readFileSync(p, 'utf-8')
}

test('gpio_input_sensor debounces and re-asserts its param against the bus', async (t) => {
  const tpl = assembled()
  t.after(() => rmSync(dirname(tpl), { recursive: true, force: true }))
  // contact-sensor composes drivers/gpio_input_sensor for its reed switch.
  const out = await generateProduct(tpl, 'contact-sensor')
  t.after(() => rmSync(out, { recursive: true, force: true }))
  const cfg = readAll(out, 'components/app_config/include/app_config.h')
  assert.match(cfg, /_INPUT_DEBOUNCE_MS\s+\d+/, 'debounce_ms lands as a config define')
  // Per-concern files: the generator splits app_logic by block category, so
  // read the whole directory rather than guess the file name.
  const logicDir = join(out, 'components/app_logic')
  const logic = readdirSync(logicDir).filter((f) => f.endsWith('.cpp'))
    .map((f) => readFileSync(join(logicDir, f), 'utf-8')).join('\n')
  assert.match(logic, /_INPUT_DEBOUNCE_POLLS/, 'polls-to-agree derived from debounce_ms / poll_ms')
  assert.match(logic, /app_driver_get_param\(/, 'reads the bus back to detect a foreign write')
  assert.match(logic, /re-asserting/, 're-asserts the debounced reading when the bus disagrees')
  assert.match(logic, /\(boot\)/, 'first poll seeds the state without debounce')
})

test('wifi_disconnect_recovery observes only unless a product opts into rebooting', async (t) => {
  const tpl = assembled()
  t.after(() => rmSync(dirname(tpl), { recursive: true, force: true }))
  // contact-sensor pulls baseline-connectivity, which carries the block.
  const out = await generateProduct(tpl, 'contact-sensor')
  t.after(() => rmSync(out, { recursive: true, force: true }))
  const cfg = readAll(out, 'components/app_config/include/app_config.h')
  assert.match(cfg, /NETRECOV_RECOV_TIMEOUT_S\s+0\b/, 'the baseline no longer opts every product into a reboot')
  const diag = readAll(out, 'components/app_logic/diagnostics.cpp')
  assert.match(diag, /#if NETRECOV_RECOV_TIMEOUT_S > 0/, 'the reboot is compiled in only for a positive timeout')
  assert.match(diag, /no reboot configured/, 'the observe-only branch logs the outage')
})
