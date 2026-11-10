// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// The engine is released before the catalog it ships with, so for a while a
// newer engine generates trees from the PREVIOUS catalog (code_blocks,
// base_firmware, product_configurations). Every product of that catalog must
// still generate a tree whose own code names nothing the catalog lacks.
//
// The previous catalog is read from git (OLD_CATALOG_REF, the commit the
// engine was last released at); a clone without that commit skips the sweep.
// The check is textual: every app_* / zc_* function called and every
// app_* / zc_* type used in a generated source is declared somewhere in the
// generated tree. ESP-IDF names are out of its reach (a real build is).
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, mkdirSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync, existsSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))
const gen = await import(join(ROOT, 'engine/dist/generator.js'))

/** The catalog the released engine was last published with. */
const OLD_CATALOG_REF = '3679fc2'

function haveRef(ref) {
  return spawnSync('git', ['-C', ROOT, 'cat-file', '-e', `${ref}^{commit}`]).status === 0
}

/** The catalog at `ref`, assembled by that commit's own assembler. */
function oldCatalog(ref) {
  const dir = mkdtempSync(join(tmpdir(), 'zc-oldcat-'))
  const tar = execFileSync('git', ['-C', ROOT, 'archive', '--format=tar', ref,
    'code_blocks', 'base_firmware', 'product_configurations', 'baselines', 'scripts/assemble_blocks.py'], { maxBuffer: 1 << 30 })
  execFileSync('tar', ['-x', '-C', dir], { input: tar })
  const templatesDir = join(dir, 'templates')
  execFileSync('python3', [join(dir, 'scripts/assemble_blocks.py'),
    '--out', join(templatesDir, 'code_blocks'), '--products-out', join(templatesDir, 'product_configurations')],
  { stdio: 'ignore', cwd: dir })
  return { dir, paths: { templatesDir, baseFirmwareDir: join(dir, 'base_firmware') } }
}

function sources(dir, out = []) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name)
    if (statSync(p).isDirectory()) sources(p, out)
    else if (/\.(c|cpp|h|hpp)$/.test(name)) out.push(p)
  }
  return out
}

/** Declared by a managed component's header, outside the generated tree. */
const EXTERNAL = /^app_network_/

const NOT_A_TYPE = new Set(['return', 'else', 'case', 'sizeof', 'if', 'while', 'for', 'switch', 'do', 'goto', 'new', 'delete', 'throw', 'not', 'and', 'or'])

/** Names a tree calls or uses as a type that it never declares. */
function undeclaredNames(files) {
  const text = files.map((f) => readFileSync(f, 'utf8')
    .replace(/\/\*[\s\S]*?\*\//g, ' ')
    .replace(/\/\/[^\n]*/g, ' ')
    .replace(/"(?:\\.|[^"\\\n])*"/g, '""')).join('\n')
  const declared = new Set()
  for (const m of text.matchAll(/#define\s+((?:app|zc)_\w+)/g)) declared.add(m[1])
  for (const m of text.matchAll(/typedef\b[^;]*?\b((?:app|zc)_\w+_t)\s*;/g)) declared.add(m[1])
  for (const m of text.matchAll(/typedef[^;]*\(\s*\*\s*((?:app|zc)_\w+_t)\s*\)/g)) declared.add(m[1])
  for (const m of text.matchAll(/\}\s*((?:app|zc)_\w+_t)\s*;/g)) declared.add(m[1])
  for (const m of text.matchAll(/\b(?:struct|enum|union)\s+((?:app|zc)_\w+)/g)) declared.add(m[1])
  // A function is declared where its name follows a type: `bool name(`, `*name(`.
  for (const m of text.matchAll(/([A-Za-z_]\w*|\*|&)\s*\b((?:app|zc)_\w+)\s*\(/g)) {
    if (!NOT_A_TYPE.has(m[1])) declared.add(m[2])
  }
  const missing = new Set()
  for (const m of text.matchAll(/\b((?:app|zc)_\w+)\s*\(/g)) if (!declared.has(m[1]) && !EXTERNAL.test(m[1])) missing.add(m[1])
  for (const m of text.matchAll(/\b((?:app|zc)_\w+_t)\b/g)) if (!declared.has(m[1])) missing.add(m[1])
  return [...missing].sort()
}

test('the checker finds a call and a type nothing declares', () => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-undecl-'))
  try {
    writeFileSync(join(dir, 'a.h'), 'bool app_known(int x);\ntypedef void (*app_cb_t)(void);\n')
    writeFileSync(join(dir, 'b.cpp'), '#include "a.h"\nstatic app_cb_t s;\nvoid f(void) { if (app_known(1)) app_unknown(2); app_gone_t *p; return app_known(3); }\n')
    assert.deepEqual(undeclaredNames(sources(dir)), ['app_gone_t', 'app_unknown'])
  } finally { rmSync(dir, { recursive: true, force: true }) }
})

test('an older base_firmware without param guards or app_utils_factory_reset gets the fallbacks', async () => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-feat-'))
  try {
    mkdirSync(join(dir, 'components/app_driver/include'), { recursive: true })
    mkdirSync(join(dir, 'components/app_utils/include'), { recursive: true })
    writeFileSync(join(dir, 'components/app_driver/include/app_driver.h'), 'bool app_driver_param_seen(app_driver_param_id_t id);\n')
    writeFileSync(join(dir, 'components/app_utils/include/app_utils.h'), 'void app_factory_reset_hook(void);\n')
    const old = await gen.detectCatalogFeatures(dir)
    assert.deepEqual(old, { takeRefusal: false, utilsFactoryReset: false })
    const matter = gen.genAppMatterCpp([], old)
    assert.match(matter, /static inline bool app_driver_take_refusal\(/)
    assert.match(matter, /esp_matter::factory_reset\(\);/)
    assert.doesNotMatch(matter, /app_utils/)
    const mqtt = gen.genAppMqttCpp([{ block: { id: 'frameworks/mqtt', kind: 'framework', slots: {} }, prefix: '', cfg: {}, slots: {} }], old)
    assert.match(mqtt, /static inline bool app_driver_take_refusal\(/)

    const now = await gen.detectCatalogFeatures(join(ROOT, 'base_firmware'))
    assert.deepEqual(now, gen.CURRENT_CATALOG)
    const current = gen.genAppMatterCpp([], now)
    assert.doesNotMatch(current, /static inline bool app_driver_take_refusal/)
    assert.match(current, /app_utils_factory_reset\(\);/)
  } finally { rmSync(dir, { recursive: true, force: true }) }
})

test('every product of the previous catalog generates with this engine and declares what it calls', { skip: !haveRef(OLD_CATALOG_REF) && `${OLD_CATALOG_REF} is not in this clone` }, async () => {
  const { dir, paths } = oldCatalog(OLD_CATALOG_REF)
  const outRoot = mkdtempSync(join(tmpdir(), 'zc-oldgen-'))
  try {
    const products = (await eng.listAllProducts(paths)).map((p) => p.product ?? p)
    const boardless = products.filter((p) => !existsSync(join(dir, 'product_configurations', p.id, 'board.yaml')))
    // At least one product per framework the platform builds, plus behaviors and buttons.
    const frameworks = new Set(boardless.flatMap((p) => p.frameworks ?? []))
    for (const fw of ['matter', 'rainmaker', 'zigbee', 'mqtt', 'webui', 'ble_mesh', 'espnow', 'ble_hid']) {
      assert.ok(frameworks.has(fw), `the previous catalog has a ${fw} product`)
    }
    const failures = []
    for (const product of boardless) {
      const outDir = join(outRoot, product.id)
      const chip = product.ci_chips?.[0] ?? 'esp32c6'
      try {
        await eng.generate(paths, { product, board: null, outDir, chip })
      } catch (e) {
        failures.push(`${product.id}: generate failed: ${e.message}`)
        continue
      }
      const missing = undeclaredNames(sources(join(outDir, 'components')).concat(sources(join(outDir, 'main'))))
      if (missing.length) failures.push(`${product.id}: undeclared ${missing.join(', ')}`)
      // An older app_logic CMakeLists lists its sources: every generated
      // behavior file must be on that list, or it is never compiled.
      const logic = join(outDir, 'components/app_logic')
      const cmake = readFileSync(join(logic, 'CMakeLists.txt'), 'utf8')
      if (!/file\(GLOB/.test(cmake)) {
        for (const f of readdirSync(logic).filter((n) => /\.(c|cpp)$/.test(n))) {
          if (!cmake.includes(`"${f}"`)) failures.push(`${product.id}: components/app_logic/${f} is not in SRCS`)
        }
      }
    }
    assert.deepEqual(failures, [])
  } finally {
    rmSync(dir, { recursive: true, force: true })
    rmSync(outRoot, { recursive: true, force: true })
  }
})
