// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Framework pairs that boot into an abort or a dead radio are refused, in
// either order, and every other mix the catalog uses still passes.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { frameworkConflicts, validateProduct } from '../dist/validator.js'

test('matter + rainmaker is refused in either order, as not supported YET', () => {
  assert.equal(frameworkConflicts(['matter', 'rainmaker']).length, 1)
  assert.equal(frameworkConflicts(['rainmaker', 'matter']).length, 1)
  // The pair works upstream (RainMaker joining through Matter): the message
  // must not read as impossible, and must still tell the writer what to do.
  assert.match(frameworkConflicts(['matter', 'rainmaker'])[0], /cannot be combined yet/)
  assert.match(frameworkConflicts(['matter', 'rainmaker'])[0], /does not build yet/)
  assert.match(frameworkConflicts(['matter', 'rainmaker'])[0], /Pick one of them\./)
})

test('ble_mesh beside matter or rainmaker is refused', () => {
  assert.match(frameworkConflicts(['matter', 'ble_mesh'])[0], /NimBLE/)
  assert.match(frameworkConflicts(['ble_mesh', 'rainmaker'])[0], /NimBLE/)
})

test('combinations the catalog ships are untouched', () => {
  for (const mix of [['matter', 'mqtt'], ['rainmaker', 'aws_iot'], ['ble_mesh'], ['matter', 'mqtt', 'display', 'webui', 'sound'], ['webui', 'agents']]) {
    assert.deepEqual(frameworkConflicts(mix), [], mix.join('+'))
  }
})

test('validateProduct reports the conflict as an error', () => {
  const blocks = new Map([['behaviors/x', { id: 'behaviors/x', kind: 'behavior', slots: {} }]])
  const r = validateProduct({ id: 'p', frameworks: ['matter', 'rainmaker'], instances: [{ block: 'behaviors/x', prefix: 'X' }] }, blocks)
  assert.ok(r.errors.some(e => /matter and rainmaker cannot be combined/.test(e.message)), JSON.stringify(r.errors))
})
