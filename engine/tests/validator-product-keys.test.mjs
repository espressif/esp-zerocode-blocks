#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * A top-level key the engine does not know is named, with the key it most
 * likely meant. The generator ignores unknown keys, and a product that asked
 * for PSRAM under `sdkconfig:` (the real key is `extra_sdkconfig:`) built
 * without it and shipped a camera that could never show a picture.
 *
 * Standalone: node's built-in runner against engine/dist.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const { validateProduct, nearestProductKey, unknownProductKeys } = await import(join(ROOT, 'engine/dist/index.js'))

const BLOCKS = new Map([
  ['behaviors/fx_tick', { id: 'behaviors/fx_tick', kind: 'behavior', description: 'ticks', params: {} }],
])
const product = (extra) => ({ id: 'fx', name: 'fx', description: 'fx', keywords: [], frameworks: [], instances: [{ block: 'behaviors/fx_tick', prefix: 'T', cfg: {} }], ...extra })

test('an unknown top-level key is a warning that names the nearest known key', () => {
  const { errors, warnings } = validateProduct(product({ sdkconfig: { CONFIG_SPIRAM: 'y' } }), BLOCKS)
  assert.equal(errors.length, 0)
  const w = warnings.find((x) => /'sdkconfig:' is not a product key/.test(x.message))
  assert.ok(w, JSON.stringify(warnings))
  assert.match(w.message, /did you mean 'extra_sdkconfig:'/)
})

test('a known key set warns about nothing; a stray key with no near match is named without a guess', () => {
  assert.deepEqual(unknownProductKeys(product({ extra_sdkconfig: { CONFIG_SPIRAM: 'y' }, product_metadata: {} })), [])
  const { warnings } = validateProduct(product({ title: 'Hub' }), BLOCKS)
  const w = warnings.find((x) => /'title:' is not a product key/.test(x.message))
  assert.ok(w)
  assert.doesNotMatch(w.message, /did you mean/)
})

test('nearestProductKey: containment either way, case and dashes folded, nothing for short junk', () => {
  assert.equal(nearestProductKey('sdkconfig'), 'extra_sdkconfig')
  assert.equal(nearestProductKey('sdkconfig_fragment'), 'sdkconfig_fragments')
  assert.equal(nearestProductKey('Extra-Sdkconfig'), 'extra_sdkconfig')
  assert.equal(nearestProductKey('chip'), undefined)
  assert.equal(nearestProductKey('id_'), undefined)
})
