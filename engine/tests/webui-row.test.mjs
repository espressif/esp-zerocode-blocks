// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/webui_row puts a product's own param on the web UI page, declaring
// the param with the type the product picks; the page's server takes product
// handlers through app_webui_register_handlers instead of a second server.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

test('a product row is on the page, its param in the enum with the chosen type, and the server takes product handlers', async () => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  try {
    const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
    const product = {
      id: 'fan', name: 'fan', frameworks: ['webui'],
      instances: [
        { block: 'device_types/fan', prefix: 'FAN', cfg: { label: 'Fan' } },
        { block: 'behaviors/webui_row', prefix: 'AUTO', cfg: { label: 'Auto', kind: 'toggle', param: 'APP_DRIVER_PARAM_FAN_AUTO', value_kind: 'bool', min: 0, max: 1 } },
        { block: 'behaviors/webui_row', prefix: 'NOW', cfg: { label: 'Speed now', kind: 'value', param: 'APP_DRIVER_PARAM_FAN_APPLIED', value_kind: 'u8' } },
      ],
    }
    const blocks = new Map((await eng.listAllBlocks(paths)).map((b) => [b.id, b.block]))
    assert.deepEqual(eng.validateProduct(product, blocks).errors.map((e) => e.message), [])
    await eng.generate(paths, { product, board: null, outDir, chip: 'esp32c6' })
    const cpp = readFileSync(join(outDir, 'components/app_webui/app_webui.cpp'), 'utf8')
    assert.match(cpp, /\{ "Auto", "toggle", APP_DRIVER_PARAM_FAN_AUTO, "APP_DRIVER_PARAM_FAN_AUTO", ZC_WEBUI_VT_bool, 0, 1, 1, 1 \}/)
    assert.match(cpp, /\{ "Speed now", "value", APP_DRIVER_PARAM_FAN_APPLIED, "APP_DRIVER_PARAM_FAN_APPLIED", ZC_WEBUI_VT_u8,/)
    assert.match(cpp, /app_webui_register_handlers\(server\);/)
    const types = readFileSync(join(outDir, 'components/app_driver/include/app_driver_types.h'), 'utf8')
    assert.match(types, /APP_DRIVER_PARAM_FAN_AUTO,\s*\/\* bool/)
    assert.match(types, /APP_DRIVER_PARAM_FAN_APPLIED,\s*\/\* u8|APP_DRIVER_PARAM_FAN_APPLIED,\s*\/\* uint8/)
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true })
    rmSync(dirname(outDir), { recursive: true, force: true })
  }
})
