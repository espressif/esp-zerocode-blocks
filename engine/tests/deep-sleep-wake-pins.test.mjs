// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/deep_sleep_cycle: a wake pin the target chip cannot wake from
// stops the build. The lists are ESP-IDF's (soc/<chip>/include/soc/
// rtc_io_channel.h), written out here independently of the block's masks.
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/deep_sleep_cycle/slots')

const range = (a, b) => Array.from({ length: b - a + 1 }, (_, i) => a + i)
const WAKE = {
  ESP32: [0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 35, 36, 37, 38, 39],
  ESP32C3: range(0, 5), ESP32C5: range(0, 6), ESP32C6: range(0, 7), ESP32S31: range(0, 7),
  ESP32H2: range(7, 14), ESP32S3: range(0, 21), ESP32P4: range(0, 15),
}

function head(wake, wake2) {
  const cfg = { wake_gpio: wake, wake_gpio_2: wake2, heartbeat_minutes: 60, max_awake_ms: 20000, quiet_ms: 1000, boot_awake_seconds: 20, stay_awake_on_usb: 1, wake_pull: 'pullup' }
  const sub = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'D', prefix_lc: 'd', cfg })
  const statics = sub('logic_statics.c')
  return sub('config_defines.h') + '\n' + statics.slice(0, statics.indexOf('/* Defined by this block'))
}

function builds(chip, wake, wake2 = -1) {
  const dir = mkdtempSync(join(tmpdir(), 'zc-dsc-'))
  try {
    writeFileSync(join(dir, 'a.c'), `#define CONFIG_IDF_TARGET_${chip} 1\n` + head(wake, wake2))
    return spawnSync(CC, ['-E', join(dir, 'a.c')], { encoding: 'utf8' }).status === 0
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
}

test('deep_sleep_cycle: every chip accepts exactly its wake-capable pins', { skip: !haveCc && 'no host C compiler' }, () => {
  for (const [chip, pins] of Object.entries(WAKE)) {
    const accepted = range(0, 48).filter((p) => builds(chip, p))
    assert.deepEqual(accepted, pins, chip)
  }
})

test('deep_sleep_cycle: the second wake pin is checked too, and no wake pin is fine', { skip: !haveCc && 'no host C compiler' }, () => {
  assert.equal(builds('ESP32H2', 9, 4), false)
  assert.equal(builds('ESP32H2', 9, 10), true)
  assert.equal(builds('ESP32H2', -1, -1), true)   /* heartbeat only */
})
