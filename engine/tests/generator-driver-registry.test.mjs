// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// The solution registry is sized from the composition. A fixed 8 ran out on a
// busy product: the 9th registrant got handle 0 (LOCAL) and its callback never
// ran, so e.g. a persistence behavior silently stopped saving.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const gen = await import(join(ROOT, 'engine/dist/generator.js'))

const fw = (id) => ({ block: { id: `frameworks/${id}`, kind: 'framework' }, prefix: id, cfg: {}, slots: {} })
const persist = (n) => ({
  block: { id: 'behaviors/nvs_persist_bool', kind: 'behavior' }, prefix: `p${n}`, cfg: {},
  slots: { logic_init: `s_p${n}_handle = app_driver_register_solution("p${n}", cb, NULL);` },
})
const relay = { block: { id: 'drivers/relay', kind: 'driver' }, prefix: 'r', cfg: {}, slots: { driver_init: 'gpio_config(&c);' } }

test('a small product keeps the old registry size', () => {
  assert.equal(gen.solutionRegistrySize([fw('matter'), relay, persist(1)]), 8)
})

test('every registrant gets a slot, with headroom for writer code', () => {
  const rendered = [fw('matter'), fw('mqtt'), relay, ...[1, 2, 3, 4, 5, 6, 7].map(persist)]
  // 2 frameworks + 7 behaviors = 9 registrants, more than the old fixed 8
  assert.equal(gen.solutionRegistrySize(rendered), 13)
})
