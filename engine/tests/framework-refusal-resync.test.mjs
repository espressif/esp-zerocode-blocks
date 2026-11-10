// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// A command a param guard refuses must not be shown as done: a RainMaker case
// writes the param and then echoes the commanded value to the app (a pump's
// tile would say ON while its E-stop holds it off), MQTT publishes the
// commanded state. Each framework handler therefore re-reports the real value
// AFTER its cases; Zigbee does it with its resync source, so a remote's
// command is never replayed; Matter rejects the write in PRE_UPDATE.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

async function gen(frameworks, file) {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  const product = { id: 'p', name: 'p', frameworks, instances: [
    { block: 'drivers/relay', prefix: 'APP', cfg: { gpio: 4, active_level: 1, param_id: 'APP_DRIVER_PARAM_POWER' } },
    { block: 'device_types/smart_plug', prefix: 'PLUG', cfg: { power_param: 'APP_DRIVER_PARAM_POWER' } }] }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c6' })
  const src = readFileSync(join(outDir, file), 'utf8')
  rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true })
  return src
}


for (const [fw, file, fn, inner, cb, source] of [
  ['rainmaker', 'components/app_rainmaker/app_rainmaker.cpp', 'rainmaker_write_cb', 'rainmaker_write_cases(', 'rainmaker_driver_cb', 'APP_DRIVER_SOURCE_LOCAL'],
  ['mqtt', 'components/app_mqtt/app_mqtt.cpp', 'mqtt_handle_data', 'mqtt_handle_data_cases(', 'mqtt_driver_cb', 'APP_DRIVER_SOURCE_LOCAL'],
  ['zigbee', 'components/app_zigbee/app_zigbee.cpp', 'zb_attribute_handler', 'zb_attribute_handler_cases(', 'zigbee_driver_cb', 'ZB_SOURCE_RESYNC'],
]) {
  test(`${fw}: a refused command is re-reported after the cases`, async () => {
    const src = await gen([fw], file)
    const at = src.indexOf(`static ${fw === 'rainmaker' ? 'esp_err_t' : 'void'} ${fn}(`)
    assert.ok(at >= 0, `${fn} defined`)
    const body = src.slice(at, src.indexOf('\n}\n', at))
    const clear = body.indexOf('app_driver_take_refusal(NULL);')
    const run = body.indexOf(inner)
    const resync = body.indexOf(`${cb}(zc_refused, zc_now, ${source}, NULL);`)
    assert.ok(clear >= 0 && run > clear && resync > run, `clear, then cases, then re-report: ${clear} ${run} ${resync}`)
  })
}

test('matter: a refused write is rejected in PRE_UPDATE', async () => {
  const src = await gen(['matter'], 'components/app_matter/app_matter.cpp')
  const pre = src.slice(src.indexOf('if (type == attribute::PRE_UPDATE) {'))
  const body = pre.slice(0, pre.indexOf('\n    }\n'))
  assert.match(body, /app_driver_take_refusal\(NULL\);[\s\S]*app_driver_set_param[\s\S]*if \(app_driver_take_refusal\(NULL\)\) return ESP_ERR_NOT_ALLOWED;/)
})
