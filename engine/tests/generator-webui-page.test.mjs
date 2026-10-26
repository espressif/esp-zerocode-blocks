// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// The webui page's own script, run against a stub DOM: a thermostat's
// target (a `number` row) and heat-only mode (a toggle whose on is 4, not 1)
// must post the values the firmware expects.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import vm from 'node:vm'
import { fileURLToPath } from 'node:url'
import { genAppWebuiCpp } from '../dist/generator.js'

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..')

function thermostatRows(cfg) {
  const src = fs.readFileSync(path.join(ROOT, 'code_blocks/device_types/thermostat/slots/webui_entities.c'), 'utf8')
  return src
    .replace(/\{\{#if cfg\.(\w+)\}\}\n([\s\S]*?)\{\{\/if\}\}\n/g, (_, k, body) => (cfg[k] ? body : ''))
    .replace(/\{\{cfg\.(\w+)\}\}/g, (_, k) => cfg[k])
}

const CFG = {
  label: 'Thermostat',
  local_temp_param: 'APP_DRIVER_PARAM_LOCAL_TEMP',
  heat_setpoint_param: 'APP_DRIVER_PARAM_HEAT_SETPOINT',
  system_mode_param: 'APP_DRIVER_PARAM_SYSTEM_MODE',
  heating_param: 'APP_DRIVER_PARAM_HEAT_RELAY',
}

function generate(cfg) {
  const rows = thermostatRows(cfg)
  return genAppWebuiCpp([{ block: { id: 'device_types/thermostat', kind: 'device_type', slots: {} }, prefix: 'THERMO', cfg, slots: { webui_entities: rows } }])
}

/** The page script, out of the INDEX_HTML C string literal. */
function pageScript(cpp) {
  const lit = cpp.slice(cpp.indexOf('INDEX_HTML[] =') + 14, cpp.indexOf(';\n\nstatic int32_t entity_value'))
  const html = [...lit.matchAll(/"((?:[^"\\]|\\.)*)"/g)].map(m => m[1].replace(/\\(.)/g, '$1')).join('')
  return html.slice(html.indexOf('<script>') + 8, html.indexOf('</script>'))
}

function el(tag) {
  return {
    tagName: tag.toUpperCase(), children: [], textContent: '', className: '',
    appendChild(c) { this.children.push(c); return c },
  }
}

/** Run the page once against `state`; returns the root and the POSTed URLs. */
async function runPage(script, state, activeElement = null) {
  const posts = []
  const root = el('div')
  root.__defineSetter__('textContent', function () { this.children = [] })
  const ctx = {
    document: { activeElement, createElement: el, getElementById: () => root },
    fetch: (url, opts) => {
      if (opts && opts.method === 'POST') { posts.push(url); return Promise.resolve({}) }
      return Promise.resolve({ json: () => state })
    },
    setInterval: () => 0, encodeURIComponent, Math,
  }
  vm.runInNewContext(script, ctx)
  await new Promise(r => setImmediate(r))
  return { root, posts }
}

const find = (root, label) => root.children.find(r => r.children[0].textContent === label)

const STATE = {
  entities: [
    { label: 'Thermostat temperature (°C)', kind: 'value', param: 'APP_DRIVER_PARAM_LOCAL_TEMP', min: -4000, max: 12500, scale: 100, step: 1, value: 1987 },
    { label: 'Thermostat target (°C)', kind: 'number', param: 'APP_DRIVER_PARAM_HEAT_SETPOINT', min: 500, max: 3500, scale: 100, step: 50, value: 2137 },
    { label: 'Thermostat heating mode', kind: 'toggle', param: 'APP_DRIVER_PARAM_SYSTEM_MODE', min: 0, max: 4, scale: 1, step: 1, value: 4 },
    { label: 'Thermostat heating now', kind: 'binary', param: 'APP_DRIVER_PARAM_HEAT_RELAY', min: 0, max: 1, scale: 1, step: 1, value: 1 },
  ],
}

test('thermostat rows: reading, target as a number in 0.5 °C steps over 5-35 °C, heat-only mode toggle, heating state', () => {
  const cpp = generate(CFG)
  assert.match(cpp, /\{ "Thermostat target \(°C\)", "number", APP_DRIVER_PARAM_HEAT_SETPOINT, "APP_DRIVER_PARAM_HEAT_SETPOINT", 2, 500, 3500, 100, 50 \}/)
  assert.match(cpp, /\{ "Thermostat heating mode", "toggle", APP_DRIVER_PARAM_SYSTEM_MODE, "APP_DRIVER_PARAM_SYSTEM_MODE", 1, 0, 4, 1, 1 \}/)
  assert.match(cpp, /"binary", APP_DRIVER_PARAM_HEAT_RELAY/)
  assert.match(cpp, /int32_t step;/)
  assert.doesNotMatch(generate({ ...CFG, heating_param: '' }), /heating now/)
})

test('the target shows in degrees and + / - post the next step on the grid, clamped to the range', async () => {
  const { root, posts } = await runPage(pageScript(generate(CFG)), structuredClone(STATE))
  const row = find(root, 'Thermostat target (°C)')
  const [minus, shown, plus] = row.children[1].children
  assert.equal(shown.textContent, '21.4')       // 2137 hundredths, one decimal for 0.5 steps
  plus.onclick()                                // off the grid: + goes to the next step above, not 0.5 past it
  assert.equal(shown.textContent, '21.5')
  assert.match(posts.at(-1), /param=APP_DRIVER_PARAM_HEAT_SETPOINT&value=2150$/)
  minus.onclick(); minus.onclick()
  assert.match(posts.at(-1), /value=2050$/)
  assert.equal(posts.length, 3)
  const top = await runPage(pageScript(generate(CFG)), { entities: [{ ...STATE.entities[1], value: 3500 }] })
  find(top.root, 'Thermostat target (°C)').children[1].children[2].onclick()
  assert.equal(top.posts.length, 0)              // already at max: nothing sent
})

test('a toggle posts its max for on and its min for off (heat-only mode: 4 / 0)', async () => {
  const { root, posts } = await runPage(pageScript(generate(CFG)), structuredClone(STATE))
  const box = find(root, 'Thermostat heating mode').children[1]
  assert.equal(box.checked, true)
  box.checked = false; box.onchange()
  assert.match(posts.at(-1), /param=APP_DRIVER_PARAM_SYSTEM_MODE&value=0$/)
  box.checked = true; box.onchange()
  assert.match(posts.at(-1), /value=4$/)
})

test('the server makes any non-min toggle value its max and snaps a stepped value', () => {
  const cpp = generate(CFG)
  assert.match(cpp, /if \(strcmp\(e->kind, "toggle"\) == 0\) raw = \(raw != e->min\) \? e->max : e->min;/)
  assert.match(cpp, /if \(e->step > 1\) raw = e->min \+ \(\(raw - e->min \+ e->step \/ 2\) \/ e->step\) \* e->step;/)
  assert.match(cpp, /\\"step\\":%ld/)
})

test('a focused checkbox does not stop the refresh; a focused slider does', async () => {
  const script = pageScript(generate(CFG))
  const box = { tagName: 'INPUT', type: 'checkbox' }
  assert.equal((await runPage(script, structuredClone(STATE), box)).root.children.length, 4)
  const range = { tagName: 'INPUT', type: 'range' }
  assert.equal((await runPage(script, structuredClone(STATE), range)).root.children.length, 0)
})

test('a reading or target never written yet shows --, not 0', async () => {
  const cpp = generate(CFG)
  assert.match(cpp, /\\"seen\\":%s\}/)
  assert.match(cpp, /app_driver_param_seen\(e->param\) \? "true" : "false"/)
  const unseen = structuredClone(STATE)
  unseen.entities[0] = { ...unseen.entities[0], value: 0, seen: false }
  unseen.entities[1] = { ...unseen.entities[1], value: 0, seen: false }
  const { root } = await runPage(pageScript(cpp), unseen)
  assert.equal(find(root, 'Thermostat temperature (°C)').children[1].textContent, '--')
  assert.equal(find(root, 'Thermostat target (°C)').children[1].children[1].textContent, '--')
  const seen = structuredClone(STATE)
  seen.entities[0] = { ...seen.entities[0], value: 0, seen: true }
  const r2 = await runPage(pageScript(cpp), seen)
  assert.equal(find(r2.root, 'Thermostat temperature (°C)').children[1].textContent, '0.00')
  assert.equal(find(r2.root, 'Thermostat target (°C)').children[1].children[1].textContent, '21.4')
})
