// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Blocks declare the device data their firmware reads (`device_data:`), and
 * the generator writes the product's `device_data.csv` from them — the file
 * the platform turns into its flash form. Checked with product-local blocks,
 * so the rules hold whatever the catalog declares.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, existsSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'
import { stringify } from 'yaml'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

function setup(t, localBlocks = {}) {
  const root = mkdtempSync(join(tmpdir(), 'zc-devdata-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const tpl = join(root, 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const local = join(root, 'local')
  for (const [name, block] of Object.entries(localBlocks)) {
    mkdirSync(join(local, name), { recursive: true })
    writeFileSync(join(local, name, 'block.yml'),
      stringify({ id: `local/${name}`, kind: 'behavior', description: name, ...block }))
  }
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware'), localBlocksDir: local }
  let n = 0
  const gen = async (product, outDir = join(root, `out-${n++}`)) => {
    const result = await eng.generate(paths, { product, board: null, outDir, chip: 'esp32c6' })
    return { result, outDir }
  }
  const validate = async (product) => {
    const blocks = new Map((await eng.listAllBlocks(paths)).map((b) => [b.id, b.block]))
    return eng.validateProduct(product, blocks)
  }
  const catalog = async (id) => (await eng.listAllProducts(paths)).find((p) => p.id === id).product
  return { gen, validate, catalog, root }
}

/** A PEM private-key armour line, assembled at runtime so the literal never
 *  trips a secrets scan. */
const pemLine = ['-----', 'BEGIN', ' ', 'PRIVATE', ' KEY-----'].join('')

const demo = (instances, frameworks = []) => ({ id: 'demo', name: 'demo', description: 'demo', frameworks, instances })
const use = (name) => ({ block: `local/${name}`, prefix: name.toUpperCase(), cfg: {} })

test("a block's declaration becomes device_data.csv at the tree root, and its rows come back", async (t) => {
  const { gen } = setup(t, {
    cloud: { device_data: { namespace: 'cloud', keys: [
      { key: 'endpoint', encoding: 'string', label: 'Endpoint' },
      { key: 'port', encoding: 'i32', default: 8883, label: 'Port', required: false },
      { key: 'cert', type: 'file', encoding: 'binary', label: 'Certificate (PEM)' },
    ] } },
  })
  const { result, outDir } = await gen(demo([use('cloud')]))
  const body = readFileSync(join(outDir, 'device_data.csv'), 'utf8').split('\n').filter((l) => l && !l.startsWith('#'))
  assert.deepEqual(body, [
    'key,type,encoding,value,label,required',
    'cloud,namespace,,,,',
    'endpoint,data,string,,Endpoint,yes',
    'port,data,i32,8883,Port,no',
    'cert,file,binary,,Certificate (PEM),yes',
  ])
  assert.deepEqual(result.deviceData[1], {
    namespace: 'cloud', key: 'port', type: 'data', encoding: 'i32', default: '8883',
    label: 'Port', required: false, from: ['local/cloud'],
  })
})

test('a product whose blocks declare nothing gets no device_data.csv', async (t) => {
  const { gen, catalog } = setup(t)
  const { result, outDir } = await gen(await catalog('smart-plug'))
  assert.equal(existsSync(join(outDir, 'device_data.csv')), false)
  assert.deepEqual(result.deviceData, [])
})

test('blocks merge: a shared key is one row, and each namespace is opened once', async (t) => {
  const { gen } = setup(t, {
    a: { device_data: { namespace: 'one', keys: [{ key: 'token', encoding: 'string', label: 'API token, v2' }] } },
    b: { device_data: { namespace: 'two', keys: [{ key: 'serial', encoding: 'string' }] } },
    c: { device_data: { namespace: 'one', keys: [
      { key: 'token', encoding: 'string', label: 'API token, v2' },
      { key: 'region', encoding: 'u8', default: 3, required: false },
    ] } },
  })
  const { result, outDir } = await gen(demo([use('a'), use('b'), use('c'), use('a')]))
  const body = readFileSync(join(outDir, 'device_data.csv'), 'utf8').split('\n').filter((l) => l && !l.startsWith('#'))
  assert.deepEqual(body, [
    'key,type,encoding,value,label,required',
    'one,namespace,,,,',
    'token,data,string,,"API token, v2",yes',
    'region,data,u8,3,region,no',
    'two,namespace,,,,',
    'serial,data,string,,serial,yes',
  ])
  assert.deepEqual(result.deviceData[0].from, ['local/a', 'local/c'])
})

test('two blocks declaring one key differently are refused, before any file is written', async (t) => {
  const { gen, validate, root } = setup(t, {
    a: { device_data: { namespace: 'one', keys: [{ key: 'token', encoding: 'string' }] } },
    b: { device_data: { namespace: 'one', keys: [{ key: 'token', type: 'file', encoding: 'binary' }] } },
  })
  const product = demo([use('a'), use('b')])
  const { errors } = await validate(product)
  assert.ok(errors.some((e) => /'one\/token' is declared differently by local\/a and local\/b/.test(e.message)),
    errors.map((e) => e.message).join('\n'))
  const outDir = join(root, 'existing-tree')
  mkdirSync(outDir)
  writeFileSync(join(outDir, 'keep'), '')
  await assert.rejects(gen(product, outDir), /declared differently by local\/a and local\/b/)
  assert.ok(existsSync(join(outDir, 'keep')), 'the existing tree is untouched')
})

test('validateBlock refuses a declaration the platform would refuse', () => {
  const check = (keys, namespace = 'ns') =>
    eng.validateBlock({ id: 'x', kind: 'behavior', device_data: { namespace, keys } }).errors.map((e) => e.message)
  assert.deepEqual(check([{ key: 'ok', encoding: 'string' }, { key: 'cert', type: 'file', encoding: 'binary' }]), [])
  const cases = [
    [[{ key: 'cert', type: 'file', encoding: 'binary', default: 'x' }], /a file key takes no default/],
    [[{ key: 'cert', type: 'file', encoding: 'i32' }], /encoding 'i32' is not one of/],
    [[{ key: 'a_key_that_is_too_long', encoding: 'string' }], /longer than 15 bytes/],
    [[{ key: '#comment', encoding: 'string' }], /only letters, digits and '_'/],
    [[{ key: 'n', encoding: 'u8', default: 300 }], /out of range for u8/],
    [[{ key: 'n', encoding: 'u8', default: 'abc' }], /not an integer/],
    [[{ key: 'h', encoding: 'hex2bin', default: 'abc' }], /even-length hex/],
    [[{ key: 'k', encoding: 'string', default: pemLine }], /holds a private key/],
    [[{ key: 'k', encoding: 'string' }, { key: 'k', encoding: 'string' }], /declared twice/],
    [[{ key: 'k', encoding: 'string', label: 'two\nlines' }], /single-line string/],
    [[{ key: 'k', encoding: 'string', required: 'yes' }], /required must be true or false/],
    [[{ key: 'k', encoding: 'string' }], /namespace 'a namespace' may use only/, 'a namespace'],
    [[], /keys must be a non-empty list/],
  ]
  for (const [keys, want, namespace] of cases) {
    const errors = check(keys, namespace)
    assert.ok(errors.some((m) => want.test(m)), `${JSON.stringify(keys)} → ${errors.join(' | ') || '(no error)'}`)
  }
})
