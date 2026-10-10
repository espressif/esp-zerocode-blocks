// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// earlyBuildComposition: frameworks + a neutral instance + what each framework
// declares under early_build, read against the board.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { mkdtempSync, mkdirSync, writeFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const { earlyBuildComposition } = await import(join(ROOT, 'engine/dist/index.js'))

function scaffold(t, { flashMb = 4, devices = '' } = {}) {
  const root = mkdtempSync(join(tmpdir(), 'zc-early-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const blocks = join(root, 'templates', 'code_blocks')
  const writeBlock = (id, body) => {
    mkdirSync(join(blocks, id), { recursive: true })
    writeFileSync(join(blocks, id, 'block.yml'), JSON.stringify({ id, kind: 'framework', description: id, ...body }))
  }
  writeBlock('frameworks/screen', {
    early_build: {
      instances: [{ block: 'drivers/stand_in_panel', prefix: 'EARLY_LCD', unless_board_has: 'display_lcd', cfg: { pin: 4 } }],
    },
  })
  writeBlock('frameworks/voice', { early_build: { partition_table: '8mb-voice', min_flash_mb: 8 } })
  writeBlock('frameworks/net', {})
  const boardDir = join(root, 'boards', 'pack', 'demo_board')
  mkdirSync(boardDir, { recursive: true })
  writeFileSync(join(boardDir, 'board_info.yaml'), 'board: demo_board\nchip: esp32s3\n')
  writeFileSync(join(boardDir, 'sdkconfig.defaults.board'), `CONFIG_ESPTOOLPY_FLASHSIZE_${flashMb}MB=y\n`)
  if (devices) writeFileSync(join(boardDir, 'board_devices.yaml'), devices)
  return {
    paths: { templatesDir: join(root, 'templates'), baseFirmwareDir: join(root, 'base'), boardsDir: join(root, 'boards') },
    board: { selected: 'demo', chip: 'esp32s3', bmgr: { board: 'demo_board', resolved_from: 'exact' } },
  }
}

test('a framework with no early_build adds only the neutral instance', async (t) => {
  const { paths, board } = scaffold(t)
  const r = await earlyBuildComposition(paths, { frameworks: ['net'], chip: 'esp32s3', board })
  assert.deepEqual(r.product.frameworks, ['net'])
  assert.deepEqual(r.product.instances.map((i) => i.block), ['behaviors/uptime_log'])
})

test('a declared instance is added when the board lacks the device', async (t) => {
  const { paths, board } = scaffold(t)
  const r = await earlyBuildComposition(paths, { frameworks: ['screen'], chip: 'esp32s3', board })
  const panel = r.product.instances.find((i) => i.block === 'drivers/stand_in_panel')
  assert.ok(panel)
  assert.deepEqual(panel.cfg, { pin: 4 })
  assert.equal('unless_board_has' in panel, false)
})

test('the board device wins over the declared instance', async (t) => {
  const { paths, board } = scaffold(t, { devices: 'devices:\n  - name: display_lcd\n    type: display_lcd\n' })
  const r = await earlyBuildComposition(paths, { frameworks: ['screen'], chip: 'esp32s3', board })
  assert.equal(r.product.instances.some((i) => i.block === 'drivers/stand_in_panel'), false)
})

test('a partition table is set, and a board below min_flash_mb is skipped', async (t) => {
  const big = scaffold(t, { flashMb: 8 })
  const ok = await earlyBuildComposition(big.paths, { frameworks: ['voice'], chip: 'esp32s3', board: big.board })
  assert.equal(ok.product.partition_table, '8mb-voice')
  const small = scaffold(t, { flashMb: 4 })
  const skip = await earlyBuildComposition(small.paths, { frameworks: ['voice'], chip: 'esp32s3', board: small.board })
  assert.match(skip.skip, /needs 8 MB of flash; the board has 4 MB/)
})
