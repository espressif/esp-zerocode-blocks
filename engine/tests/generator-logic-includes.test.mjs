// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Concern files open with each #include once, in first-seen order — three
// behaviors that each bring <nvs_flash.h> no longer stack three copies.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { genAppLogic } from '../dist/generator.js'

function inst(id, prefix, includes) {
  const slots = { logic_includes: includes, logic_init: '{\n    (void)0;\n}' }
  return { block: { id, kind: 'behavior', slots }, prefix, cfg: {}, slots }
}

test('logic includes are deduplicated per line', () => {
  const out = genAppLogic([
    inst('behaviors/factory_reset_long_press', 'FR1', '#include <iot_button.h>\n#include <nvs_flash.h>\n#include <esp_system.h>'),
    inst('behaviors/factory_reset_power_cycle', 'FR2', '#include <nvs_flash.h>\n#include <esp_system.h>\n#include <esp_timer.h>'),
  ])
  const part = out.parts.find(p => p.name === 'factory_reset.cpp')
  assert.ok(part)
  const count = (re) => (part.content.match(re) || []).length
  assert.equal(count(/#include <nvs_flash\.h>/g), 1)
  assert.equal(count(/#include <esp_system\.h>/g), 1)
  assert.equal(count(/#include <esp_timer\.h>/g), 1)
  assert.ok(part.content.indexOf('<iot_button.h>') < part.content.indexOf('<esp_timer.h>'), 'order kept')
})
