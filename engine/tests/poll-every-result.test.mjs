// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// app_driver_poll_every() refuses a poll when its table is full. A block that
// throws the result away keeps running without the work the poll does (a
// setting never saved to flash), with nothing in the log to say so.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { readdirSync, readFileSync, statSync } from 'node:fs'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))

function files(dir, out = []) {
  for (const n of readdirSync(dir)) {
    const p = join(dir, n)
    if (statSync(p).isDirectory()) files(p, out)
    else if (/\.(c|cpp|h)$/.test(n)) out.push(p)
  }
  return out
}

test('no block discards the result of app_driver_poll_every', () => {
  const bad = []
  for (const f of files(join(ROOT, 'code_blocks'))) {
    const src = readFileSync(f, 'utf8').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '')
    for (const m of src.matchAll(/^[ \t]*(\(void\)\s*)?app_driver_poll_every\s*\(/gm)) bad.push(`${f.slice(ROOT.length + 1)}: ${m[0].trim()}`)
  }
  assert.deepEqual(bad, [])
})
