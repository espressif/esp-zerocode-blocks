#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * The audio flash budget on 8mb-voice: audio alone generates for every chip
 * that claims it (the ~1.35 MB image fits the 2.25 MB slot on the S3, P4 and
 * S31 alike on esp-sr 2.4.x — a per-chip refusal here would be measuring
 * esp-sr 2.5's esp-dl payload, which the audio block deliberately does not
 * pin), the combo rule still holds, and an all-local ESP32-P4 tree carries no
 * hosted radio stack. Generates over the REAL assembled catalog.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

function assembled() {
  const out = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(out, 'code_blocks'), '--products-out', join(out, 'product_configurations')], { stdio: 'ignore' })
  return out
}

async function gen(templatesDir, id, chip) {
  const paths = { templatesDir, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const product = await eng.loadProduct(paths, id)
  const outDir = mkdtempSync(join(tmpdir(), `zc-out-${id}-`))
  try {
    return await eng.generate(paths, { product, board: null, outDir, chip })
  } finally {
    rmSync(outDir, { recursive: true, force: true })
  }
}

test('audio alone on the 8mb-voice table generates for every chip that claims it', async (t) => {
  const tpl = assembled()
  t.after(() => rmSync(dirname(tpl), { recursive: true, force: true }))
  for (const chip of ['esp32s3', 'esp32p4', 'esp32s31']) {
    const r = await gen(tpl, 'voice-lamp', chip)
    assert.ok(r.blocks.length > 0, `${chip} generates`)
  }
  // The combo rule is unchanged: audio + a big transport still needs 16mb-voice.
  // voice-matter-lamp ships on 16mb-voice; the same product on 8mb-voice must be refused.
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const combo = { ...(await eng.loadProduct(paths, 'voice-matter-lamp')), partition_table: '8mb-voice' }
  const outDir = mkdtempSync(join(tmpdir(), 'zc-out-combo-'))
  t.after(() => rmSync(outDir, { recursive: true, force: true }))
  await assert.rejects(eng.generate(paths, { product: combo, board: null, outDir, chip: 'esp32s3' }),
    (e) => e.message.includes('matter') && e.message.includes('16mb-voice'), 'audio+matter on 8mb-voice is refused')
})

test('an all-local product on the esp32p4 carries no hosted radio stack; a networked one does', async (t) => {
  const tpl = assembled()
  t.after(() => rmSync(dirname(tpl), { recursive: true, force: true }))
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const manifest = async (id) => {
    const product = await eng.loadProduct(paths, id)
    const outDir = mkdtempSync(join(tmpdir(), `zc-out-${id}-`))
    t.after(() => rmSync(outDir, { recursive: true, force: true }))
    await eng.generate(paths, { product, board: null, outDir, chip: 'esp32p4' })
    return readFileSync(join(outDir, 'main/idf_component.yml'), 'utf-8')
  }
  const local = await manifest('voice-lamp-16mb')          // audio only
  for (const pkg of eng.P4_HOSTED_PACKAGES) assert.ok(!local.includes(pkg), `${pkg} absent from a local tree`)
  const networked = await manifest('smart-plug')            // matter
  for (const pkg of eng.P4_HOSTED_PACKAGES) assert.ok(networked.includes(pkg), `${pkg} kept for a radio tree`)
})

test('the 16mb-voice table passes the per-chip gate', async (t) => {
  const tpl = assembled()
  t.after(() => rmSync(dirname(tpl), { recursive: true, force: true }))
  const r = await gen(tpl, 'voice-lamp-16mb', 'esp32p4')
  assert.ok(r.blocks.length > 0)
})
