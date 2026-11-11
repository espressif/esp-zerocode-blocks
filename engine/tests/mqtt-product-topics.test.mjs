// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// frameworks/mqtt product topics: app_mqtt_publish / app_mqtt_subscribe ride
// the framework's one connection (a second client took the same default id
// and the broker dropped the two in turn), subscriptions are renewed on every
// connect, and MQTT wildcards match as the spec says.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))
const CXX = process.env.CXX || 'c++'
const haveCxx = spawnSync(CXX, ['--version']).status === 0

async function generated() {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  const product = { id: 'p', name: 'p', frameworks: ['mqtt'], instances: [
    { block: 'drivers/relay', prefix: 'APP', cfg: { gpio: 4, active_level: 1, param_id: 'APP_DRIVER_PARAM_POWER' } },
    { block: 'device_types/smart_plug', prefix: 'PLUG', cfg: { power_param: 'APP_DRIVER_PARAM_POWER' } }] }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c3' })
  const src = readFileSync(join(outDir, 'components/app_mqtt/app_mqtt.cpp'), 'utf8')
  rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true })
  return src
}

test('mqtt: product topics are subscribed on connect and dispatched beside entity commands', async () => {
  const src = await generated()
  assert.match(src, /esp_err_t app_mqtt_publish\(/)
  assert.match(src, /esp_err_t app_mqtt_subscribe\(/)
  const conn = src.slice(src.indexOf('case MQTT_EVENT_CONNECTED:'), src.indexOf('case MQTT_EVENT_DISCONNECTED:'))
  assert.match(conn, /zc_topics_on_connected\(\);/, 'subscriptions renewed on every connect')
  const data = src.slice(src.indexOf('static void mqtt_handle_data'), src.indexOf('static void mqtt_event_handler'))
  assert.match(data, /zc_topics_dispatch\(topic, tlen, data, dlen\);/)
})

test('mqtt: topic filters match with + and # as MQTT defines them', { skip: !haveCxx && 'no host C++ compiler' }, async () => {
  const src = await generated()
  const a = src.indexOf('static bool zc_topic_matches')
  const fn = src.slice(a, src.indexOf('\n}\n', a) + 3)
  const dir = mkdtempSync(join(tmpdir(), 'zc-mt-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), `#include <cstdio>\n#include <cstring>\n${fn}\nstatic int m(const char *f, const char *t) { return zc_topic_matches(f, t, (int)strlen(t)); }\nint main() { printf("%d%d%d%d%d%d%d%d\\n", m("a/b", "a/b"), m("a/b", "a/bc"), m("a/+/c", "a/x/c"), m("a/+/c", "a/x/y/c"), m("a/#", "a/x/y"), m("a/#", "b/x"), m("+/b", "a/b"), m("a/b", "a")); return 0; }\n`)
    execFileSync(CXX, ['-std=c++17', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '10101010')
  } finally { rmSync(dir, { recursive: true, force: true }) }
})
