// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/ble_mesh_provisioner: max_nodes above the stack's ceiling
// (CONFIG_BLE_MESH_MAX_PROV_NODES) stops the build instead of tracking nodes
// the stack refuses to provision.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))
const CC = process.env.CC || 'cc'
const haveCc = spawnSync(CC, ['--version']).status === 0
const slot = readFileSync(join(ROOT, 'code_blocks/behaviors/ble_mesh_provisioner/slots/ble_mesh_statics.c'), 'utf8')
const head = slot.slice(0, slot.indexOf('typedef struct'))

function preprocess(maxNodes) {
  const dir = mkdtempSync(join(tmpdir(), 'zc-prov-'))
  try {
    writeFileSync(join(dir, 'sdkconfig.h'), '#define CONFIG_BLE_MESH_MAX_PROV_NODES 20\n')
    writeFileSync(join(dir, 'a.c'), eng.__substituteForTest(head, { prefix: 'P', prefix_lc: 'p', cfg: { max_nodes: maxNodes, group_addr: '0xC000', app_key: '00' } }))
    return spawnSync(CC, ['-E', '-I', dir, join(dir, 'a.c')], { encoding: 'utf8' }).status
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
}

test('ble_mesh_provisioner: max_nodes within the stack ceiling builds, above it stops', { skip: !haveCc && 'no host C compiler' }, () => {
  assert.equal(preprocess(8), 0)
  assert.equal(preprocess(20), 0)
  assert.notEqual(preprocess(25), 0)
  assert.notEqual(preprocess(0), 0)
})
