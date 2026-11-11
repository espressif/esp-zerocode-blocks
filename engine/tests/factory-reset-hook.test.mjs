// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// base_firmware's factory reset: app_utils_factory_reset() runs the product's
// app_factory_reset_hook() before erasing NVS, a product definition in C or in
// C++ (through app_utils.h) replaces the weak default, and every factory-reset
// path in the base and the blocks goes through it.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const UTILS = join(ROOT, 'base_firmware/components/app_utils')
const CXX = process.env.CXX || 'c++'
const CC = process.env.CC || 'cc'
const haveCc = spawnSync(CXX, ['--version']).status === 0 && spawnSync(CC, ['--version']).status === 0

/* The two functions under test, cut out of app_utils.cpp as they are. */
function resetSource() {
  const src = readFileSync(join(UTILS, 'app_utils.cpp'), 'utf-8')
  const m = src.match(/\/\* C linkage from the declaration[\s\S]*?\nesp_err_t app_utils_factory_reset\(void\)\n\{[\s\S]*?\n\}\n/)
  assert.ok(m, 'app_factory_reset_hook + app_utils_factory_reset in app_utils.cpp')
  return m[0]
}

const STUB_HEADERS = {
  'esp_err.h': '#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n',
  'driver/ledc.h': '#pragma once\ntypedef struct { int freq_hz; } ledc_timer_config_t;\n',
}

const MAIN = (body) => String.raw`
#include <stdio.h>
#include <string.h>
#include "app_utils.h"
extern "C" char g_trace[64];
char g_trace[64];
static const char *esp_err_to_name(esp_err_t) { return "err"; }
#define ESP_LOGW(tag, fmt, ...) ((void)(tag))
#define ESP_LOGI(tag, fmt, ...) ((void)(tag))
#define ESP_LOGE(tag, fmt, ...) ((void)(tag))
static esp_err_t g_erase_result = ESP_OK;
static esp_err_t nvs_flash_erase(void) { strcat(g_trace, "erase,"); return g_erase_result; }
static void esp_restart(void) { strcat(g_trace, "restart"); }
${body}
int main(void)
{
    esp_err_t r = app_utils_factory_reset();
    printf("%s|%d\n", g_trace, r);
    g_trace[0] = 0;
    g_erase_result = 7;
    r = app_utils_factory_reset();
    printf("%s|%d\n", g_trace, r);
    return 0;
}
`

function build(t, products) {
  const dir = mkdtempSync(join(tmpdir(), 'zc-fr-hook-'))
  t.after(() => rmSync(dir, { recursive: true, force: true }))
  for (const [p, c] of Object.entries(STUB_HEADERS)) {
    mkdirSync(dirname(join(dir, 'inc', p)), { recursive: true })
    writeFileSync(join(dir, 'inc', p), c)
  }
  const inc = ['-I', join(UTILS, 'include'), '-I', join(dir, 'inc')]
  writeFileSync(join(dir, 'main.cpp'), MAIN(resetSource()))
  const objs = []
  execFileSync(CXX, ['-std=c++17', '-Wall', '-Werror', '-Wno-unused-function', ...inc, '-c', join(dir, 'main.cpp'), '-o', join(dir, 'main.o')], { stdio: 'pipe' })
  objs.push(join(dir, 'main.o'))
  for (const [name, code] of Object.entries(products)) {
    writeFileSync(join(dir, name), code)
    const cc = name.endsWith('.c') ? [CC, '-std=c11'] : [CXX, '-std=c++17']
    execFileSync(cc[0], [cc[1], '-Wall', '-Werror', ...inc, '-c', join(dir, name), '-o', join(dir, `${name}.o`)], { stdio: 'pipe' })
    objs.push(join(dir, `${name}.o`))
  }
  execFileSync(CXX, [...objs, '-o', join(dir, 'p')], { stdio: 'pipe' })
  const run = spawnSync(join(dir, 'p'), { encoding: 'utf-8' })
  assert.equal(run.status, 0, run.stdout + run.stderr)
  return run.stdout.trim().split('\n')
}

const CXX_HOOK = 'extern "C" char g_trace[64];\nvoid app_factory_reset_hook(void) { strcat(g_trace, "hook,"); }\n'
const HOOK_BODY = 'extern char g_trace[64];\nvoid app_factory_reset_hook(void) { strcat(g_trace, "hook,"); }\n'

test('the default hook does nothing: erase, then reboot; a failed erase returns its error, no reboot', { skip: !haveCc && 'no host C/C++ compiler' }, (t) => {
  assert.deepEqual(build(t, {}), ['erase,restart|0', 'erase,|7'])
})

test('a product hook in a C file runs before the erase', { skip: !haveCc && 'no host C/C++ compiler' }, (t) => {
  const c = `#include <string.h>\n#include "app_utils.h"\n${HOOK_BODY}`
  assert.deepEqual(build(t, { 'reset_extra.c': c }), ['hook,erase,restart|0', 'hook,erase,|7'])
})

test('a product hook in a C++ file that includes app_utils.h runs before the erase', { skip: !haveCc && 'no host C/C++ compiler' }, (t) => {
  const cpp = `#include <string.h>\n#include "app_utils.h"\n${CXX_HOOK}`
  assert.deepEqual(build(t, { 'reset_extra.cpp': cpp }), ['hook,erase,restart|0', 'hook,erase,|7'])
})

test('without the header a C++ definition is another symbol and never runs (why the header says include it)', { skip: !haveCc && 'no host C/C++ compiler' }, (t) => {
  const cpp = `#include <string.h>\n${CXX_HOOK}`
  assert.deepEqual(build(t, { 'reset_extra.cpp': cpp }), ['erase,restart|0', 'erase,|7'])
})

test('every factory reset in the base console and the blocks goes through app_utils_factory_reset', () => {
  const console = readFileSync(join(ROOT, 'base_firmware/components/app_console/app_console.cpp'), 'utf-8')
  const cmd = console.match(/static int cmd_factory_reset\([^)]*\)\n\{[\s\S]*?\n\}/)[0]
  assert.match(cmd, /app_utils_factory_reset\(\)/)
  assert.doesNotMatch(cmd, /nvs_flash_erase/)
  const paths = []
  const walk = (d) => {
    for (const e of readdirSync(d, { withFileTypes: true })) {
      const p = join(d, e.name)
      if (e.isDirectory()) walk(p)
      else if (/\/slots\/[^/]+\.(c|h|cpp)$/.test(p)) paths.push(p)
    }
  }
  walk(join(ROOT, 'code_blocks'))
  const resetters = paths.filter((p) => /factory reset/i.test(readFileSync(p, 'utf-8')) && /app_utils_factory_reset\(\)/.test(readFileSync(p, 'utf-8')))
  for (const b of ['factory_reset_long_press', 'factory_reset_button_combo', 'factory_reset_power_cycle', 'board_button']) {
    assert.ok(resetters.some((p) => p.includes(`/${b}/`)), `${b} calls app_utils_factory_reset()`)
  }
  for (const p of paths) {
    const code = readFileSync(p, 'utf-8').replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '')
    assert.doesNotMatch(code, /\bnvs_flash_erase\s*\(\s*\)/, p)
  }
})
