// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// A light driver writes the state it boots in (off, at its default level and
// colour) onto every param it owns that nothing has written yet, so a hub or
// app reads it from the start instead of "never written" (level 0 while the
// first switch-on comes up at full).
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

const DRIVERS = [
  [{ block: 'drivers/pwm_led', prefix: 'L', cfg: { gpio: 4 } }, ['POWER', 'BRIGHTNESS']],
  [{ block: 'drivers/pwm_led_2ch', prefix: 'L', cfg: { warm_gpio: 4, cool_gpio: 5 } }, ['POWER', 'BRIGHTNESS', 'COLOR_TEMP']],
  [{ block: 'drivers/pwm_rgb', prefix: 'L', cfg: { red_gpio: 4, green_gpio: 5, blue_gpio: 6 } }, ['POWER', 'BRIGHTNESS', 'HUE', 'SATURATION']],
  [{ block: 'drivers/ws2812_strip', prefix: 'L', cfg: { gpio: 4, num_leds: 8 } }, ['POWER', 'BRIGHTNESS', 'HUE', 'SATURATION']],
]

test('each light driver writes its boot state onto every param it owns', async (t) => {
  const root = mkdtempSync(join(tmpdir(), 'zc-light-boot-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const tpl = join(root, 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
  for (const [driver, params] of DRIVERS) {
    const outDir = join(root, driver.block.split('/')[1])
    await eng.generate(paths, { product: { id: 'l', name: 'l', frameworks: [], instances: [driver] }, board: null, outDir, chip: 'esp32c6' })
    const cpp = readFileSync(join(outDir, 'components/app_driver/app_driver.cpp'), 'utf8')
    for (const p of params) {
      assert.match(cpp, new RegExp(`if \\(!app_driver_param_seen\\(APP_DRIVER_PARAM_${p}\\)\\)[\\s\\S]*?app_driver_set_param\\(APP_DRIVER_PARAM_${p}, \\w+, APP_DRIVER_SOURCE_RESTORE\\)`), `${driver.block}: ${p}`)
    }
  }
})
