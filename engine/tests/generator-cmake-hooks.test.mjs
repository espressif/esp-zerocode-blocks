// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// A selected framework's cmake/ hooks are copied into the tree; an unselected
// framework's are not.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, existsSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { generate, FRAMEWORK_CMAKE_HOOKS } from '../dist/index.js'

const framework = (name) => `id: frameworks/${name}
kind: framework
description: fixture framework ${name}
slots: {}
`

function scaffold(t) {
  const root = mkdtempSync(join(tmpdir(), 'zc-hooks-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))

  const base = join(root, 'base_firmware')
  mkdirSync(join(base, 'main'), { recursive: true })
  writeFileSync(join(base, 'main/app_main.cpp'), 'extern "C" void app_main(void)\n{\n{{main_init}}\n}\n')
  writeFileSync(join(base, 'main/idf_component.yml'), 'dependencies: {}\n')
  writeFileSync(join(base, 'partitions.csv'), 'factory, app, factory, 0x10000, 0x100000,\n')
  writeFileSync(join(base, 'sdkconfig.defaults'), '# base\n')

  const templates = join(root, 'templates')
  for (const name of ['fx_a', 'fx_b']) {
    const dir = join(templates, 'code_blocks/frameworks', name)
    mkdirSync(join(dir, 'cmake'), { recursive: true })
    writeFileSync(join(dir, 'block.yml'), framework(name))
    for (const hook of FRAMEWORK_CMAKE_HOOKS) writeFileSync(join(dir, 'cmake', hook), `# ${name} ${hook}\n`)
  }
  mkdirSync(join(templates, 'product_configurations'), { recursive: true })

  return {
    paths: { templatesDir: templates, baseFirmwareDir: base, boardsDir: join(root, 'boards') },
    outDir: join(root, 'out'),
  }
}

test('only the selected framework\'s hooks reach cmake/frameworks/<name>/', async (t) => {
  const { paths, outDir } = scaffold(t)
  await generate(paths, {
    product: { id: 'fx', name: 'fx', description: 'fx', frameworks: ['fx_a'], instances: [] },
    board: null,
    chip: 'esp32c6',
    outDir,
  })
  for (const hook of FRAMEWORK_CMAKE_HOOKS) {
    assert.equal(readFileSync(join(outDir, 'cmake/frameworks/fx_a', hook), 'utf8'), `# fx_a ${hook}\n`)
  }
  assert.equal(existsSync(join(outDir, 'cmake/frameworks/fx_b')), false)
})
