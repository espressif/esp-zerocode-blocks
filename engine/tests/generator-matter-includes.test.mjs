// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// app_matter.cpp says `using namespace esp_matter` and uses node_t, attribute::
// and factory_reset() itself, so it must include <esp_matter.h> on its own. It
// used to arrive only through a device-type block's matter_includes slot: a
// Matter product with no device type yet (the framework alone) did not compile.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { genAppMatterCpp } from '../dist/generator.js'

test('app_matter.cpp includes esp_matter.h with no device-type blocks', () => {
  const out = genAppMatterCpp([])
  assert.match(out, /#include <esp_matter\.h>/)
  assert.ok(out.indexOf('#include <esp_matter.h>') < out.indexOf('using namespace esp_matter;'), 'included before use')
})
