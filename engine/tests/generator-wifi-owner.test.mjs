// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Exactly one framework brings up Wi-Fi in a product, whatever the mix.
// esp_wifi_init() refuses a second call, and ESP_ERROR_CHECK turns that into a
// boot abort; aws_iot decides at runtime and defers to whoever initialised it.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { genAppMqttCpp, genAppAgentsCpp, genAppWebuiCpp, wifiOwner } from '../dist/generator.js'

function fw(name) {
  return { block: { id: `frameworks/${name}`, kind: 'framework', slots: {} }, prefix: name.toUpperCase(), cfg: {}, slots: {} }
}

const GEN = { mqtt: genAppMqttCpp, agents: genAppAgentsCpp, webui: genAppWebuiCpp }
const WIFI = ['matter', 'rainmaker', 'mqtt', 'agents', 'aws_iot', 'webui']

// Which generated files call esp_wifi_init (aws_iot's static component counts
// only when nobody else does: it defers at runtime).
function initialisers(names) {
  const rendered = names.map(fw)
  const out = names.filter(n => GEN[n] && /esp_wifi_init\(/.test(GEN[n](rendered)))
  return out
}

test('every pair of codegen Wi-Fi frameworks initialises Wi-Fi exactly once', () => {
  for (const a of WIFI) {
    for (const b of WIFI) {
      if (a === b) continue
      const names = [a, b]
      const owner = wifiOwner(names.map(fw))
      const inits = initialisers(names)
      // matter / rainmaker / aws_iot bring Wi-Fi up outside these generators.
      const expected = GEN[owner] ? [owner] : []
      assert.deepEqual(inits, expected, `${names.join('+')}: owner ${owner}, initialisers ${inits.join(',')}`)
    }
  }
})

test('webui beside aws_iot serves on the station instead of starting a SoftAP', () => {
  const out = genAppWebuiCpp([fw('webui'), fw('aws_iot')])
  assert.doesNotMatch(out, /WIFI_MODE_AP/)
  assert.doesNotMatch(out, /esp_wifi_init\(/)
})

test('webui beside agents leaves Wi-Fi to agents', () => {
  assert.equal(wifiOwner([fw('webui'), fw('agents')]), 'agents')
  assert.doesNotMatch(genAppWebuiCpp([fw('webui'), fw('agents')]), /esp_wifi_init\(/)
  assert.match(genAppAgentsCpp([fw('webui'), fw('agents')]), /esp_wifi_init\(/)
})

test('standalone mqtt tolerates a default loop aws_iot created first', () => {
  const out = genAppMqttCpp([fw('aws_iot'), fw('mqtt')])
  assert.doesNotMatch(out, /ESP_ERROR_CHECK\(esp_event_loop_create_default\(\)\)/)
  assert.match(out, /loop_err != ESP_ERR_INVALID_STATE/)
})
