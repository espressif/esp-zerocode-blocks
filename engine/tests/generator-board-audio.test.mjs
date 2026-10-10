#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * drivers/board_audio_codec — the board's own codec, asked for by name — and
 * the sound framework driving ANY speaker that implements the tone contract.
 *
 * Guarding:
 *   - the block states no pin, I2S port or I2C address: on a board those are
 *     the board's, and a hand-pinned codec there is an IO conflict;
 *   - it declares esp_codec_dev itself, because the board manager pulls it in
 *     only behind a Kconfig gate (two products' builds lost it);
 *   - a missing device fails GENERATION, like every board adapter;
 *   - the sound framework's sequencer drives whichever speaker block the
 *     product composed — it used to look for drivers/speaker_i2s by id, so a
 *     product with speaker_es8311 (mosaico-touch-hub) played nothing.
 *
 * Runs against the assembled real catalog and a minimal board pack in a temp
 * dir, so no board-pack checkout is needed.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

function setup(t) {
  const root = mkdtempSync(join(tmpdir(), 'zc-baudio-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const tpl = join(root, 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const board = join(root, 'boards', 'pack', 'demo_s31_audio')
  mkdirSync(board, { recursive: true })
  writeFileSync(join(board, 'board_info.yaml'), 'board: demo_s31_audio\nchip: esp32s31\n')
  writeFileSync(join(board, 'sdkconfig.defaults.board'), 'CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y\n')
  writeFileSync(join(board, 'board_devices.yaml'),
    'devices:\n  - name: audio_dac\n    chip: es8311\n    type: audio_codec\n    config:\n      dac_enabled: true\n' +
    '  - name: audio_adc\n    chip: es8311\n    type: audio_codec\n    config:\n      adc_enabled: true\n      dac_enabled: false\n' +
    '  - name: boot_button\n    type: button\n')
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware'), boardsDir: join(root, 'boards') }
  let n = 0
  const gen = async (product) => {
    const outDir = join(root, `out-${n++}`)
    await eng.generate(paths, {
      product,
      board: { schema: 'zc-board/1', selected: 'demo', chip: 'esp32s31', bmgr: { board: 'demo_s31_audio', resolved_from: 'exact' } },
      outDir, chip: 'esp32s31',
    })
    return (rel) => readFileSync(join(outDir, rel), 'utf8')
  }
  return { gen }
}

const code = (src) => src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '')
const speaker = (device = 'audio_dac') => ({
  block: 'drivers/board_audio_codec', prefix: 'SPK',
  cfg: { device, tone_param: 'APP_DRIVER_PARAM_TONE_HZ', volume_param: 'APP_DRIVER_PARAM_VOLUME' },
})
const product = (instances, frameworks = ['sound']) => ({ id: 'demo', name: 'demo', description: 'demo', frameworks, instances })

test('board_audio_codec asks the board for its codec by name and states no pin, port or address', async (t) => {
  const { gen } = setup(t)
  const read = await gen(product([speaker()]))
  const drv = code(read('components/app_driver/app_driver.cpp'))
  assert.match(drv, /esp_board_manager_get_device_handle\(SPK_ACODEC_DEVICE,/)
  assert.match(drv, /esp_codec_dev_open\(/)
  assert.match(drv, /mclk_multiple/, 'the MCLK ratio is taken from the board, not left at the default')
  assert.ok(!/i2s_new_channel|audio_codec_new_i2c_ctrl|gpio_config/.test(drv), 'it brings up nothing the board owns')
  assert.match(read('components/app_config/include/app_config.h'), /#define SPK_ACODEC_DEVICE\s+"audio_dac"/)
  assert.match(read('main/idf_component.yml'), /espressif\/esp_codec_dev:\s*">=2\.0\.0-beta1,<3\.0\.0"/,
    'the block declares the codec library itself')
})

test('the sound framework drives board_audio_codec through its tone param', async (t) => {
  const { gen } = setup(t)
  const snd = code((await gen(product([speaker()])))('components/app_sound/app_sound.cpp'))
  assert.match(snd, /app_driver_set_param\(APP_DRIVER_PARAM_TONE_HZ,/)
  assert.ok(!/no speaker instance/.test(snd), 'not the silent no-op')
})

test('a codec the board does not declare fails generation, naming the ones it has', async (t) => {
  const { gen } = setup(t)
  await assert.rejects(gen(product([speaker('speaker_out')])), /board device 'speaker_out' of type 'audio_codec'[\s\S]*'audio_dac'/)
})

test("the board's microphone codec (same type, DAC off) is refused — playing into it is silence", async (t) => {
  const { gen } = setup(t)
  await assert.rejects(gen(product([speaker('audio_adc')])),
    /needs its audio_codec device to have dac_enabled: true[\s\S]*'audio_adc' with dac_enabled = false[\s\S]*qualify: 'audio_dac'/)
})
