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
        { block: 'behaviors/webui_row', prefix: 'AT', cfg: { label: 'Feed at', kind: 'time', param: 'APP_DRIVER_PARAM_FEED_AT', value_kind: 'u16', min: 0, max: 1439 } },
      ],
    }
    const blocks = new Map((await eng.listAllBlocks(paths)).map((b) => [b.id, b.block]))
    assert.deepEqual(eng.validateProduct(product, blocks).errors.map((e) => e.message), [])
    await eng.generate(paths, { product, board: null, outDir, chip: 'esp32c6' })
    const cpp = readFileSync(join(outDir, 'components/app_webui/app_webui.cpp'), 'utf8')
    assert.match(cpp, /\{ "Auto", "toggle", APP_DRIVER_PARAM_FAN_AUTO, "APP_DRIVER_PARAM_FAN_AUTO", ZC_WEBUI_VT_bool, 0, 1, 1, 1 \}/)
    assert.match(cpp, /\{ "Speed now", "value", APP_DRIVER_PARAM_FAN_APPLIED, "APP_DRIVER_PARAM_FAN_APPLIED", ZC_WEBUI_VT_u8,/)
    assert.match(cpp, /app_webui_register_handlers\(server\);/)
    assert.match(cpp, /\{ "Feed at", "time", APP_DRIVER_PARAM_FEED_AT, "APP_DRIVER_PARAM_FEED_AT", ZC_WEBUI_VT_u16, 0, 1439, 1, 1 \}/)
    assert.match(cpp, /strcmp\(e->kind, "time"\) != 0\) \{/, 'a time row takes writes from the page')
    // the page script, out of its C string literal, must parse as JavaScript
    const html = [...cpp.slice(cpp.indexOf('static const char INDEX_HTML[] ='), cpp.indexOf('</html>";') + 9).matchAll(/"((?:[^"\\]|\\.)*)"/g)]
      .map((m) => m[1].replace(/\\'/g, "'").replace(/\\"/g, '"').replace(/\\\\/g, '\\')).join('')
    const script = html.slice(html.indexOf('<script>') + 8, html.indexOf('</script>'))
    assert.ok(script.includes("e.kind=='time'"), 'the page draws a time row')
    assert.doesNotThrow(() => new Function(script))
    const types = readFileSync(join(outDir, 'components/app_driver/include/app_driver_types.h'), 'utf8')
    assert.match(types, /APP_DRIVER_PARAM_FAN_AUTO,\s*\/\* bool/)
    assert.match(types, /APP_DRIVER_PARAM_FAN_APPLIED,\s*\/\* u8|APP_DRIVER_PARAM_FAN_APPLIED,\s*\/\* uint8/)
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true })
    rmSync(dirname(outDir), { recursive: true, force: true })
  }
})

test('a time row writes 0-1439 whatever its min and max, and refuses a param too small for it', async () => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  try {
    const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
    const blocks = new Map((await eng.listAllBlocks(paths)).map((b) => [b.id, b.block]))
    const product = (vk) => ({
      id: 'feeder', name: 'feeder', frameworks: ['webui'],
      instances: [
        { block: 'device_types/smart_plug', prefix: 'PLUG', cfg: { label: 'Plug' } },
        { block: 'behaviors/webui_row', prefix: 'AT', cfg: { label: 'Feed at', kind: 'time', param: 'APP_DRIVER_PARAM_FEED_AT', ...(vk ? { value_kind: vk } : {}) } },
      ],
    })
    assert.match(eng.validateProduct(product(undefined), blocks).errors.map((e) => e.message).join('\n'), /time row.*value_kind u8 cannot hold/)
    assert.match(eng.validateProduct(product('bool'), blocks).errors.map((e) => e.message).join('\n'), /value_kind bool cannot hold/)
    assert.deepEqual(eng.validateProduct(product('u16'), blocks).errors.map((e) => e.message), [])
    await eng.generate(paths, { product: product('u16'), board: null, outDir, chip: 'esp32c6' })
    const cpp = readFileSync(join(outDir, 'components/app_webui/app_webui.cpp'), 'utf8')
    // the row keeps the block's default max (100); the write path ignores it for a time row
    assert.match(cpp, /\{ "Feed at", "time", APP_DRIVER_PARAM_FEED_AT, "APP_DRIVER_PARAM_FEED_AT", ZC_WEBUI_VT_u16, 0, 100, 1, 1 \}/)
    const set = cpp.slice(cpp.indexOf('static esp_err_t set_post'), cpp.indexOf('static httpd_handle_t s_server'))
    // run the clamp as C would, through a JS transcription of the emitted lines
    assert.match(set, /const long hi = is_time \? 1439 : e->max;/)
    assert.match(set, /const long lo = is_time \? 0 : e->min;/)
    assert.match(set, /if \(raw > hi\) raw = hi;/)
    assert.doesNotMatch(set, /if \(raw > e->max\)/)
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true })
    rmSync(dirname(outDir), { recursive: true, force: true })
  }
})

test('a device type row may leave out step: the page and the write path take 0 as 1', async () => {
  const { genAppWebuiCpp } = await import(join(ROOT, 'engine/dist/generator.js'))
  const cpp = genAppWebuiCpp([{ block: { id: 'device_types/smart_plug', kind: 'device_type', slots: {} }, prefix: 'P', cfg: {},
    slots: { webui_entities: '{ "Plug", "toggle", APP_DRIVER_PARAM_POWER, "APP_DRIVER_PARAM_POWER", 0, 0, 1, 1 },' } }])
  assert.match(cpp, /var st=e\.step>0\?e\.step:1;/)
  assert.match(cpp, /if \(e->step > 1\) raw =/)
})

test('device type rows carry a step only when it is not 1, so an engine without the field still compiles them', async () => {
  const { readdirSync, existsSync } = await import('node:fs')
  const dir = join(ROOT, 'code_blocks/device_types')
  const bad = []
  for (const dt of readdirSync(dir)) {
    const f = join(dir, dt, 'slots/webui_entities.c')
    if (!existsSync(f)) continue
    for (const line of readFileSync(f, 'utf8').split('\n')) {
      const m = /^\{ (.*) \},$/.exec(line.trim())
      if (!m) continue
      const fields = m[1].replace(/"(?:\\.|[^"\\])*"/g, '""').split(',').map((s) => s.trim())
      if (fields.length === 9 && fields[8] === '1') bad.push(`${dt}: ${line.trim()}`)
      if (fields.length !== 8 && fields.length !== 9) bad.push(`${dt}: ${fields.length} fields: ${line.trim()}`)
    }
  }
  assert.deepEqual(bad, [])
})
