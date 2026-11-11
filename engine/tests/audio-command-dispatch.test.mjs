// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// A recognised voice command is dispatched on the command id it was
// registered with (esp_mn_commands_add), never on MultiNet's phrase_id — the
// phrase's index in its list, which differs from the id once a second device
// or a synonym phrase is registered.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('voice commands dispatch on command_id, the id their cases are keyed by', async (t) => {
  const root = mkdtempSync(join(tmpdir(), 'zc-audio-dispatch-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const tpl = join(root, 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(root, 'out')
  const product = {
    id: 'v', name: 'v', frameworks: ['audio'], partition_table: '8mb-voice',
    instances: [
      { block: 'drivers/relay', prefix: 'FAN', cfg: { gpio: 4, param_id: 'APP_DRIVER_PARAM_POWER' } },
      { block: 'device_types/light', prefix: 'LAMP', cfg: {} },
    ],
  }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32s3' })
  const cpp = readFileSync(join(outDir, 'components/app_audio/app_audio.cpp'), 'utf8')
  assert.match(cpp, /audio_apply_command\(mn->command_id\[0\]\)/)
  assert.doesNotMatch(cpp, /audio_apply_command\(mn->phrase_id/)
  assert.match(cpp, /esp_mn_commands_add\(/)
})
