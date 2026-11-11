// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/telegram_notify needs a network someone else brings up; the
// validator refuses a product where nobody does. The generated tree declares
// the C API in app_config.h, and app_logic / app_driver compile every source
// in their directory, so a product file needs no CMakeLists edit.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'
import { networkIssues, redundantNetwork, stationConflicts, validateProduct } from '../dist/validator.js'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

const blocks = new Map([
  ['behaviors/telegram_notify', { id: 'behaviors/telegram_notify', kind: 'behavior', needs_internet: true, singleton: true, slots: {} }],
  ['behaviors/wifi_station', { id: 'behaviors/wifi_station', kind: 'behavior', provides_internet: true, singleton: true, slots: {} }],
  ['frameworks/mqtt', { id: 'frameworks/mqtt', kind: 'framework', provides_internet: true, slots: {} }],
  ['frameworks/zigbee', { id: 'frameworks/zigbee', kind: 'framework', slots: {} }],
])
const fws = (...names) => new Map(names.map((n) => [n, blocks.get(`frameworks/${n}`)]))
const tg = { block: 'behaviors/telegram_notify', prefix: 'TG' }
const wifi = { block: 'behaviors/wifi_station', prefix: 'WIFI' }

test('a block that needs internet with nothing providing it is refused', () => {
  const out = networkIssues([tg], blocks, fws('zigbee'))
  assert.equal(out.length, 1)
  assert.match(out[0], /behaviors\/telegram_notify needs internet/)
  assert.match(out[0], /add behaviors\/wifi_station, or a framework that does \(mqtt\)/)
})

test('wifi_station or a Wi-Fi framework satisfies it', () => {
  assert.deepEqual(networkIssues([wifi, tg], blocks, fws()), [])
  assert.deepEqual(networkIssues([tg], blocks, fws('mqtt')), [])
})

test('a singleton listed twice is refused', () => {
  const out = networkIssues([wifi, tg, { ...tg, prefix: 'TG2' }], blocks, fws())
  assert.deepEqual(out, ['behaviors/telegram_notify may be listed once; it is listed 2 times'])
})

test('wifi_station beside a Wi-Fi framework is a warning, not an error', () => {
  assert.equal(redundantNetwork([wifi, tg], blocks, fws('mqtt')).length, 1)
  assert.deepEqual(redundantNetwork([wifi, tg], blocks, fws()), [])
  const r = validateProduct({ id: 'p', frameworks: ['mqtt'], instances: [wifi, tg] }, blocks)
  assert.deepEqual(r.errors.filter((e) => /internet|listed once/.test(e.message)), [])
  assert.ok(r.warnings.some((w) => /redundant beside mqtt/.test(w.message)))
})

test('wifi_station where it cannot join a network is refused: aws_iot alone, espnow, a webui access point', () => {
  const all = new Map([...blocks,
    ['frameworks/aws_iot', { id: 'frameworks/aws_iot', kind: 'framework', provides_internet: true, slots: {} }],
    ['frameworks/espnow', { id: 'frameworks/espnow', kind: 'framework', slots: {} }],
    ['frameworks/webui', { id: 'frameworks/webui', kind: 'framework', slots: {} }],
  ])
  const err = (frameworks) => stationConflicts([wifi, tg], all, frameworks)
  assert.match(err(['aws_iot']).join(), /not needed beside aws_iot/)
  assert.match(err(['espnow']).join(), /beside espnow/)
  assert.match(err(['webui']).join(), /beside webui alone/)
  // beside a framework that joins the network in its own init, or with none: no error
  assert.deepEqual(err(['mqtt']), [])
  assert.deepEqual(err(['webui', 'mqtt']), [])
  assert.deepEqual(err(['aws_iot', 'mqtt']), [])
  assert.deepEqual(err([]), [])
  // aws_iot alone is no longer called redundant (it is the error above)
  const fwm = (...names) => new Map(names.map((n) => [n, all.get(`frameworks/${n}`)]))
  assert.deepEqual(redundantNetwork([wifi, tg], all, fwm('aws_iot')), [])
  const r = validateProduct({ id: 'p', frameworks: ['aws_iot'], instances: [wifi, tg] }, all)
  assert.ok(r.errors.some((e) => /not needed beside aws_iot/.test(e.message)))
  assert.ok(!r.warnings.some((w) => /redundant/.test(w.message)))
})

test('the washer product generates with the API declared and every source globbed', async (t) => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  t.after(() => rmSync(dirname(tpl), { recursive: true, force: true }))
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const entry = (await eng.listAllProducts(paths)).find((p) => p.product.id === 'washer-done-telegram')
  assert.ok(entry, 'washer-done-telegram in the catalog')
  const out = mkdtempSync(join(tmpdir(), 'zc-out-tg-'))
  t.after(() => rmSync(out, { recursive: true, force: true }))
  await eng.generate(paths, { product: entry.product, board: null, outDir: out, chip: 'esp32c3' })
  const read = (rel) => readFileSync(join(out, rel), 'utf-8')

  const cfg = read('components/app_config/include/app_config.h')
  assert.match(cfg, /extern "C" \{[\s\S]*esp_err_t telegram_notify_send\(const char \*text\);[\s\S]*\}/)
  assert.match(cfg, /esp_err_t telegram_notify_send_keyed\(const char \*key, const char \*text\);/)
  assert.match(cfg, /#define TG_TG_MAX_RETRY_S\s+60\b/)
  assert.match(cfg, /esp_err_t telegram_notify_on_reply\(telegram_notify_reply_cb_t cb, void \*ctx\);/)

  for (const comp of ['app_logic', 'app_driver']) {
    const cmake = read(`components/${comp}/CMakeLists.txt`)
    assert.match(cmake, /file\(GLOB \w+ CONFIGURE_DEPENDS/, `${comp} globs its sources`)
    assert.match(cmake, /if\(NOT CMAKE_SCRIPT_MODE_FILE\)/, `${comp} skips the glob in script mode`)
    assert.doesNotMatch(cmake, /SRCS\s+"/, `${comp} lists no source by name`)
  }
  assert.match(read('components/app_logic/CMakeLists.txt'), /^\s*PRIV_REQUIRES[^)]*esp_http_client[^)]*espressif__json_parser/m)
  assert.match(read('main/idf_component.yml'), /espressif\/json_parser/)
  assert.match(read('sdkconfig.defaults'), /CONFIG_MBEDTLS_GCM_C=y/)
  const control = read('components/app_logic/control.cpp')
  assert.match(control, /xTaskCreate\(tg_task, "telegram", 10240/)
  assert.match(control, /app_driver_register_solution\("tg_telegram", tg_trigger_cb, NULL\)/)
})

test('on a chip with Thread and no Wi-Fi, Matter brings no internet: a block that needs it is refused', async (t) => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const out = mkdtempSync(join(tmpdir(), 'zc-out-'))
  t.after(() => { rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(out, { recursive: true, force: true }) })
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const product = {
    id: 'plug-tg', name: 'plug-tg', frameworks: ['matter'],
    instances: [
      { block: 'drivers/relay', prefix: 'APP', cfg: { gpio: 4, param_id: 'APP_DRIVER_PARAM_POWER' } },
      { block: 'device_types/smart_plug', prefix: 'PLUG', cfg: {} },
      { block: 'behaviors/telegram_notify', prefix: 'TG', cfg: {} },
    ],
  }
  await assert.rejects(eng.generate(paths, { product, board: null, outDir: join(out, 'h2'), chip: 'esp32h2' }), /needs internet over Wi-Fi, and esp32h2 has no Wi-Fi/)
  await eng.generate(paths, { product, board: null, outDir: join(out, 'c6'), chip: 'esp32c6' })
})
