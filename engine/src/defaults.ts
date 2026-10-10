// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Default blocks: behaviors every product gets without listing them. The base
 * list lives in `base_firmware/defaults.yml`; each framework block adds its own
 * under `defaults:`. A product that lists a default keeps its own instance, and
 * `exclude:` in product.yml leaves one out.
 */
import { promises as fs } from 'node:fs'
import * as path from 'node:path'
import { parse as parseYaml } from 'yaml'
import type { Block, Instance, Product } from './types.js'

export const BASE_DEFAULTS_FILE = 'defaults.yml'

/** The base default list from `<baseFirmwareDir>/defaults.yml`; [] when absent. */
export async function loadBaseDefaults(baseFirmwareDir: string): Promise<string[]> {
  let text: string
  try {
    text = await fs.readFile(path.join(baseFirmwareDir, BASE_DEFAULTS_FILE), 'utf-8')
  } catch (e) {
    if ((e as NodeJS.ErrnoException).code === 'ENOENT') return []
    throw e
  }
  const doc = (parseYaml(text) ?? {}) as { defaults?: unknown }
  if (doc.defaults === undefined || doc.defaults === null) return []
  if (!Array.isArray(doc.defaults) || doc.defaults.some((d) => typeof d !== 'string')) {
    throw new Error(`${BASE_DEFAULTS_FILE}: 'defaults' must be a list of block ids`)
  }
  return doc.defaults as string[]
}

/** Every default that applies to `product`: the base list, then each selected
 *  framework's, without repeats. `frameworkBlocks` is keyed by framework name. */
export function defaultBlockIds(
  product: Product,
  frameworkBlocks: Map<string, Block>,
  baseDefaults: string[],
): string[] {
  const ids = [...baseDefaults]
  for (const name of product.frameworks ?? []) ids.push(...(frameworkBlocks.get(name)?.defaults ?? []))
  return [...new Set(ids)]
}

function defaultPrefix(blockId: string, used: Set<string>): string {
  const base = (blockId.split('/').pop() ?? blockId).toUpperCase().replace(/[^A-Z0-9_]/g, '_')
  let prefix = base
  for (let n = 2; used.has(prefix); n++) prefix = `${base}_${n}`
  return prefix
}

/** `product` with its defaults appended as ordinary instances (default cfg).
 *  A default the product already lists, under any prefix, is not added again. */
export function withDefaults(
  product: Product,
  frameworkBlocks: Map<string, Block>,
  baseDefaults: string[],
): Product {
  const instances = product.instances ?? []
  const listed = new Set(instances.map((i) => i.block))
  const excluded = new Set(product.exclude ?? [])
  const used = new Set(instances.map((i) => i.prefix))
  const added: Instance[] = []
  for (const id of defaultBlockIds(product, frameworkBlocks, baseDefaults)) {
    if (listed.has(id) || excluded.has(id)) continue
    const prefix = defaultPrefix(id, used)
    used.add(prefix)
    added.push({ block: id, prefix, cfg: {} })
  }
  return added.length ? { ...product, instances: [...instances, ...added] } : product
}

/** Names in `exclude:` that are not a default of this product. */
export function unknownExcludes(
  product: Product,
  frameworkBlocks: Map<string, Block>,
  baseDefaults: string[],
): string[] {
  const defaults = new Set(defaultBlockIds(product, frameworkBlocks, baseDefaults))
  return (product.exclude ?? []).filter((id) => !defaults.has(id))
}

/** Problems with a default list: each id must be a chip-agnostic behavior in
 *  `blocks` that needs no cfg. `owner` names the list in messages. */
export function validateDefaults(ids: string[], blocks: Map<string, Block>, owner: string): string[] {
  const problems: string[] = []
  for (const id of ids) {
    const block = blocks.get(id)
    if (!block) problems.push(`${owner}: default '${id}' is not a block`)
    else if (block.kind !== 'behavior') problems.push(`${owner}: default '${id}' is a ${block.kind}, not a behavior`)
    else if (block.target) problems.push(`${owner}: default '${id}' is for ${block.target} only`)
    else {
      for (const [name, p] of Object.entries(block.params ?? {})) {
        if (p.required && p.default === undefined) problems.push(`${owner}: default '${id}' needs cfg.${name}`)
      }
    }
  }
  return problems
}
