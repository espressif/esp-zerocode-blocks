// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// An instance's `frameworks:` keeps it to those of the product's frameworks.
// A hub that shows a light on its own page composes device_types/light, and
// without the scope the MQTT framework would also announce the hub as a light
// Home Assistant could command, with a state that is not real.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, existsSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

async function gen(scope) {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'p')
  const light = { block: 'device_types/light', prefix: 'LAMP', cfg: { power_param: 'APP_DRIVER_PARAM_POWER' }, ...(scope ? { frameworks: scope } : {}) }
  const product = { id: 'p', name: 'p', frameworks: ['mqtt', 'webui'], instances: [light] }
  try {
    await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c6' })
    const read = (f) => existsSync(join(outDir, f)) ? readFileSync(join(outDir, f), 'utf8') : ''
    return { mqtt: read('components/app_mqtt/app_mqtt.cpp'), webui: read('components/app_webui/app_webui.cpp') }
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true })
  }
}

test('instance frameworks: a light kept to the web UI is not announced over MQTT', async () => {
  const all = await gen(null)
  assert.match(all.mqtt, /lamp/i, 'unscoped: MQTT announces the light')
  const scoped = await gen(['webui'])
  assert.doesNotMatch(scoped.mqtt, /lamp/i, 'scoped: nothing of the light in MQTT')
  assert.equal(scoped.webui, all.webui, 'the web UI is unchanged')
})

test('instance frameworks: naming a framework the product does not select is refused', async () => {
  await assert.rejects(() => gen(['matter']), /names frameworks: \[matter\], which the product does not select/)
})
