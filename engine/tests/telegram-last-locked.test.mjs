// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/telegram_notify: s_tg_last is written by the Telegram task and by
// the console (tg-learn) and read by tg-status, so every access sits between
// a take and a give of s_tg_lock. A structural check: the block needs the
// whole HTTP stack, which does not build on the host.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const src = readFileSync(join(ROOT, 'code_blocks/behaviors/telegram_notify/slots/logic_statics.c'), 'utf8')

test('telegram_notify: every read and write of s_tg_last holds s_tg_lock', () => {
  const lines = src.split('\n')
  let held = false
  const bare = []
  lines.forEach((line, i) => {
    if (/xSemaphoreTake\(s_tg_lock/.test(line)) held = true
    if (/xSemaphoreGive\(s_tg_lock/.test(line)) held = false
    const code = line.replace(/\/\*.*?\*\//g, '').replace(/\/\/.*$/, '').replace(/sizeof\(s_tg_last\)/g, '')
    if (!/\bs_tg_last\b/.test(code)) return                   // only its size, or a comment
    if (/^static char s_tg_last\[/.test(code)) return          // the definition
    if (!held) bare.push(`${i + 1}: ${line.trim()}`)
  })
  assert.deepEqual(bare, [])
})
