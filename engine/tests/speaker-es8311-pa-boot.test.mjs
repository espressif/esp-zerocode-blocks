// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/speaker_es8311: a control pin holds its intended level the moment
// it becomes an output, so an active-LOW amplifier enable is never switched
// on at boot before the codec is ready.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const CXX = process.env.CXX || 'c++'
const haveCxx = spawnSync(CXX, ['--version']).status === 0

test('speaker_es8311: a control pin is at its level before it becomes an output', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const src = readFileSync(join(ROOT, 'code_blocks/drivers/speaker_es8311/slots/driver_statics.c'), 'utf8')
  const fn = src.slice(src.indexOf('static void {{prefix_lc}}_ctrl_pin'), src.indexOf('\n}\n', src.indexOf('static void {{prefix_lc}}_ctrl_pin')) + 3)
    .replaceAll('{{prefix_lc}}', 'spk')
  const dir = mkdtempSync(join(tmpdir(), 'zc-es-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), String.raw`
#include <cstdio>
#include <cstdint>
typedef int gpio_num_t;
static int g_level[64]; static int g_level_at_config = -1;
static void gpio_set_level(gpio_num_t n, int l) { g_level[n] = l; }
enum { GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE = 0, GPIO_PULLDOWN_DISABLE = 0, GPIO_INTR_DISABLE = 0 };
struct gpio_config_t { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; };
static int gpio_config(const gpio_config_t *) { g_level_at_config = g_level[7]; return 0; }
${fn}
int main() { spk_ctrl_pin(7, 1); printf("%d\n", g_level_at_config); return 0; }
`)
    execFileSync(CXX, ['-std=c++17', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '1', 'the OFF level of an active-low PA is in place before output mode')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
