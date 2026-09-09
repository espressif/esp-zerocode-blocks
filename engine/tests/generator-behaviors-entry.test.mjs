// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// The composed behaviors are wired in a generated file of their own; the
// product's app_logic.cpp carries none of the concern calls, so rewriting it
// wholesale cannot silently drop every composed behavior.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { genAppLogic } from '../dist/generator.js'

function inst(id, prefix, slots) {
  return { block: { id, kind: 'behavior', slots }, prefix, cfg: {}, slots }
}

test('behavior init lives in zc_behaviors.cpp, not app_logic.cpp', () => {
  const rendered = [
    inst('behaviors/console_uptime', 'UP', { logic_init: '{\n    register_uptime();\n}' }),
    inst('behaviors/nvs_persist_u8', 'PB', { logic_init: '{\n    restore_brightness();\n}' }),
  ]
  const out = genAppLogic(rendered)
  const entry = out.parts.find(p => p.name === 'zc_behaviors.cpp')
  assert.ok(entry, 'zc_behaviors.cpp is generated')
  assert.match(entry.content, /esp_err_t zc_behaviors_init\(void\)/)
  assert.match(entry.content, /console_logic_init\(\);/)
  assert.match(entry.content, /persist_logic_init\(\);/)
  assert.doesNotMatch(out.orchestrator, /_logic_init\(\);/, 'app_logic.cpp calls no concern init')
  assert.match(out.orchestrator, /esp_err_t app_logic_init\(void\)/)
})

test('a product with no behaviors still gets an (empty) entry point', () => {
  const out = genAppLogic([])
  const entry = out.parts.find(p => p.name === 'zc_behaviors.cpp')
  assert.ok(entry)
  assert.match(entry.content, /zc_behaviors_init\(void\)/)
})
