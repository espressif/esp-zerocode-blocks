// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/display_st7789_spi: an ST7789 module's settings (window gap,
// inversion, subpixel order) reach ESP-IDF's ST7789 driver from template.yaml.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('display_st7789_spi: gap, inversion and colour order reach the panel', async () => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  try {
    const product = { id: 'p', name: 'p', frameworks: ['display'], instances: [
      { block: 'drivers/display_st7789_spi', prefix: 'LCD', cfg: { sclk_gpio: 12, mosi_gpio: 11, dc_gpio: 9, cs_gpio: 10, hres: 135, vres: 240, x_gap: 52, y_gap: 40, invert: 1, bgr: 0 } },
    ] }
    await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32s3' })
    const src = readFileSync(join(outDir, 'components/app_display/app_display.cpp'), 'utf8')
    const cfg = readFileSync(join(outDir, 'components/app_config/include/app_config.h'), 'utf8')
    assert.match(src, /esp_lcd_new_panel_st7789\(/)
    assert.match(src, /esp_lcd_panel_set_gap\(lcd_panel, LCD_LCD_X_GAP, LCD_LCD_Y_GAP\)/)
    assert.match(src, /esp_lcd_panel_invert_color\(lcd_panel, LCD_LCD_INVERT != 0\)/)
    assert.match(src, /LCD_LCD_BGR \? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB/)
    for (const [k, v] of [['X_GAP', 52], ['Y_GAP', 40], ['INVERT', 1], ['BGR', 0], ['HRES', 135], ['VRES', 240]]) {
      assert.match(cfg, new RegExp(`#define LCD_LCD_${k}\\s+${v}\\b`))
    }
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true })
  }
})
