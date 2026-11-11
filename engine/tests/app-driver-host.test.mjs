// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// The param bus's own host tests (base_firmware/components/app_driver/test):
// the real app_driver_cb.cpp over FreeRTOS shims, one binary per test.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { spawnSync } from 'node:child_process'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const CXX = process.env.CXX || 'c++'
const haveCxx = spawnSync(CXX, ['--version']).status === 0

test('app_driver host tests pass', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const r = spawnSync('bash', [join(ROOT, 'base_firmware/components/app_driver/test/run.sh')], { encoding: 'utf8' })
  assert.equal(r.status, 0, r.stdout + r.stderr)
})
