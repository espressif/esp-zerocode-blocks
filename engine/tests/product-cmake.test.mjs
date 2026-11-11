// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Product code in components/app_logic that needs another component lists it
// in components/app_logic/product.cmake (PRODUCT_PRIV_REQUIRES), which the
// generated CMakeLists includes when present. The engine never writes that
// file. It must hold in IDF's requirement scan too, which runs every
// CMakeLists in script mode (cmake -P) with idf_component_register redefined
// to record the requirements.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { existsSync, mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))
const haveCmake = spawnSync('cmake', ['--version']).status === 0

/* IDF's script-mode scan, reduced to what it reads: the PRIV_REQUIRES list. */
function scanPrivRequires(componentDir) {
  const script = join(mkdtempSync(join(tmpdir(), 'zc-scan-')), 'scan.cmake')
  writeFileSync(script, `function(idf_component_register)
    cmake_parse_arguments(_ "WHOLE_ARCHIVE" "" "SRCS;INCLUDE_DIRS;REQUIRES;PRIV_REQUIRES" \${ARGN})
    message("PRIV_REQUIRES=\${__PRIV_REQUIRES}")
endfunction()
include("${join(componentDir, 'CMakeLists.txt')}")
`)
  const r = spawnSync('cmake', ['-P', script], { encoding: 'utf8' })
  rmSync(dirname(script), { recursive: true, force: true })
  assert.equal(r.status, 0, r.stderr)
  const m = /PRIV_REQUIRES=(.*)/.exec(r.stderr)
  assert.ok(m, r.stderr)
  return m[1].split(';').filter(Boolean)
}

async function generateProduct(id) {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const entry = (await eng.listAllProducts(paths)).find((p) => (p.product?.id ?? p.id) === id)
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), id)
  await eng.generate(paths, { product: entry.product, board: null, outDir, chip: 'esp32c3' })
  return { outDir, cleanup: () => { rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true }) } }
}

test('app_logic takes the product\'s own PRODUCT_PRIV_REQUIRES, in the script-mode scan too', { skip: !haveCmake && 'no cmake' }, async () => {
  const { outDir, cleanup } = await generateProduct('probe-thermostat')
  try {
    const logic = join(outDir, 'components/app_logic')
    assert.equal(existsSync(join(logic, 'product.cmake')), false, 'the engine never writes product.cmake')
    const text0 = readFileSync(join(logic, 'CMakeLists.txt'), 'utf8')
    assert.match(text0, /file\(GLOB \w+ CONFIGURE_DEPENDS "\$\{CMAKE_CURRENT_LIST_DIR\}\/product\.cmake"\)/,
      'a product.cmake added to a configured tree re-runs CMake')
    const plain = scanPrivRequires(logic)
    assert.ok(plain.includes('app_driver'), plain.join(' '))
    assert.ok(!plain.includes('esp_http_client'))
    assert.ok(plain.includes('espressif__ds18b20'), 'a block\'s cmake_priv_requires still lands in the real list')
    const text = readFileSync(join(logic, 'CMakeLists.txt'), 'utf8')
    for (const line of text.split('\n').filter((l) => l.trimStart().startsWith('#'))) {
      assert.doesNotMatch(line, /espressif__ds18b20/, 'never in a comment')
    }
    writeFileSync(join(logic, 'product.cmake'), 'set(PRODUCT_PRIV_REQUIRES esp_http_client json)\n')
    const withProduct = scanPrivRequires(logic)
    assert.ok(withProduct.includes('esp_http_client') && withProduct.includes('json'), withProduct.join(' '))
    assert.ok(withProduct.includes('app_driver'), 'the generated requirements stay')
  } finally {
    cleanup()
  }
})

test('a base without an app_logic CMakeLists gets the same product.cmake hook', async (t) => {
  const root = mkdtempSync(join(tmpdir(), 'zc-pcm-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const base = join(root, 'base_firmware')
  mkdirSync(join(base, 'main'), { recursive: true })
  writeFileSync(join(base, 'main/app_main.cpp'), 'extern "C" void app_main(void)\n{\n{{main_init}}\n}\n')
  writeFileSync(join(base, 'main/idf_component.yml'), 'dependencies: {}\n')
  writeFileSync(join(base, 'partitions.csv'), 'factory, app, factory, 0x10000, 0x100000,\n')
  writeFileSync(join(base, 'sdkconfig.defaults'), '# base\n')
  const templates = join(root, 'templates')
  mkdirSync(join(templates, 'code_blocks'), { recursive: true })
  mkdirSync(join(templates, 'product_configurations'), { recursive: true })
  const outDir = join(root, 'out')
  await eng.generate({ templatesDir: templates, baseFirmwareDir: base, boardsDir: join(root, 'boards') }, {
    product: { id: 'p', name: 'p', description: 'p', frameworks: [], instances: [] }, board: null, chip: 'esp32c6', outDir,
  })
  const cmake = readFileSync(join(outDir, 'components/app_logic/CMakeLists.txt'), 'utf8')
  assert.match(cmake, /include\("\$\{CMAKE_CURRENT_LIST_DIR\}\/product\.cmake" OPTIONAL\)/)
  assert.match(cmake, /file\(GLOB \w+ CONFIGURE_DEPENDS "\$\{CMAKE_CURRENT_LIST_DIR\}\/product\.cmake"\)/)
  assert.match(cmake, /PRIV_REQUIRES [^)]*\$\{PRODUCT_PRIV_REQUIRES\}/)
  if (haveCmake) {
    writeFileSync(join(outDir, 'components/app_logic/product.cmake'), 'set(PRODUCT_PRIV_REQUIRES esp_http_client)\n')
    assert.ok(scanPrivRequires(join(outDir, 'components/app_logic')).includes('esp_http_client'))
  }
})

test('an older engine merging block requirements into the first requirement line it finds lands on the real one', () => {
  // The older engine's merge: the first `PRIV_REQUIRES <list>` anywhere, comments included.
  const unanchored = /(PRIV_REQUIRES\s+)([^\n)]*)/
  for (const component of ['app_logic', 'app_driver', 'app_utils', 'app_console', 'app_config']) {
    const file = join(ROOT, 'base_firmware/components', component, 'CMakeLists.txt')
    if (!existsSync(file)) continue
    const text = readFileSync(file, 'utf8')
    const m = unanchored.exec(text)
    if (!m) continue
    const line = text.slice(text.lastIndexOf('\n', m.index) + 1, text.indexOf('\n', m.index))
    assert.doesNotMatch(line, /^\s*#/, `${component}: the first match is a comment: ${line}`)
  }
})
