// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/buzzer_melody: the player walks a note table until a note with
// ms == 0, so every table must end with one — one without it ran past the end
// of the array on every alarm.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))

test('buzzer_melody: every note table ends with a {0,0} terminator', () => {
  const src = readFileSync(join(ROOT, 'code_blocks/drivers/buzzer_melody/slots/driver_statics.c'), 'utf8')
  const tables = [...src.matchAll(/_note_t\s+(s_\S+?)\[\]\s*=\s*\{([\s\S]*?)\};/g)]
  assert.ok(tables.length >= 5, 'found the note tables')
  for (const [, name, body] of tables) {
    const notes = [...body.matchAll(/\{\s*(\d+)\s*,\s*(\d+)\s*\}/g)].map((m) => [Number(m[1]), Number(m[2])])
    assert.deepEqual(notes.at(-1), [0, 0], `${name} ends with {0,0}`)
    assert.ok(notes.slice(0, -1).every(([, ms]) => ms > 0), `${name} has no early terminator`)
  }
})
