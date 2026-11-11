// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// frameworks/display: every touch reaches app_display_on_touch() through the
// input device (before any object can swallow it), and product code in
// app_logic can include app_display.h to define it.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('display: a weak any-touch hook on every pointer input device, reachable from app_logic', async () => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'disp')
  try {
    const product = {
      id: 'disp', name: 'disp', frameworks: ['display'],
      instances: [
        { block: 'drivers/display_ili9341_spi', prefix: 'LCD', cfg: { sclk_gpio: 12, mosi_gpio: 11, dc_gpio: 9, cs_gpio: 10 } },
        { block: 'drivers/display_touch_ft5x06', prefix: 'TP', cfg: { sda_gpio: 8, scl_gpio: 18 } },
      ],
    }
    await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32s3' })
    const src = readFileSync(join(outDir, 'components/app_display/app_display.cpp'), 'utf8')
    assert.match(src, /__attribute__\(\(weak\)\) void app_display_on_touch\(void\)/)
    assert.match(src, /lv_indev_get_type\(in\) == LV_INDEV_TYPE_POINTER\)[\s\S]*?lv_indev_add_event_cb\(in, zc_display_touch_cb, LV_EVENT_PRESSED/)
    assert.ok(src.indexOf('lv_indev_add_event_cb') > src.indexOf('lvgl_port_add_touch'), 'attached after the touch device is registered')
    assert.match(readFileSync(join(outDir, 'components/app_logic/CMakeLists.txt'), 'utf8'), /PRIV_REQUIRES[^\n]*\bapp_display\b/)
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true })
  }
})
