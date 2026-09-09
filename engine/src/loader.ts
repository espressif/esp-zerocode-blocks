// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * YAML loaders + catalog walkers for the block-composition template system.
 *
 * Knows the on-disk layout:
 *   <templatesDir>/code_blocks/<kind-plural>/<name>/block.yml
 *   <templatesDir>/product_configurations/<id>/product.yml
 */

import { promises as fs } from 'node:fs'
import * as path from 'node:path'
import { parse as parseYaml } from 'yaml'
import {
  type Block,
  type BoardFile,
  type BoardResolution,
  type Product,
  type BlockKind,
  BOARD_RESOLUTIONS,
  BOARD_SCHEMA,
  DIR_TO_BLOCK_KIND,
  BLOCK_KIND_DIRS,
} from './types.js'

export interface CatalogPaths {
  /** Absolute path to firmware/templates/. */
  templatesDir: string
  /** Optional dir of product-local blocks, resolved from `local/<name>` refs
   *  as `<localBlocksDir>/<name>/block.yml` (the block declares its own kind). */
  localBlocksDir?: string
}

/** Prefix marking a block ref as product-local rather than catalog. */
export const LOCAL_BLOCK_PREFIX = 'local/'

export function blocksDir(paths: CatalogPaths): string {
  return path.join(paths.templatesDir, 'code_blocks')
}

export function productsDir(paths: CatalogPaths): string {
  return path.join(paths.templatesDir, 'product_configurations')
}

function blockFile(paths: CatalogPaths, blockId: string): string {
  if (blockId.startsWith(LOCAL_BLOCK_PREFIX)) {
    if (!paths.localBlocksDir) {
      throw new Error(
        `block '${blockId}' is product-local but no localBlocksDir is configured`,
      )
    }
    return path.join(paths.localBlocksDir, blockId.slice(LOCAL_BLOCK_PREFIX.length), 'block.yml')
  }
  return path.join(blocksDir(paths), blockId, 'block.yml')
}

/** Load a block by its id (`<kind-plural>/<name>`, or `local/<name>`). */
export async function loadBlock(paths: CatalogPaths, blockId: string): Promise<Block> {
  const text = await fs.readFile(blockFile(paths, blockId), 'utf-8')
  return parseYaml(text) as Block
}

/** Read raw block.yml text for the given id. */
export async function loadBlockText(paths: CatalogPaths, blockId: string): Promise<string> {
  return fs.readFile(blockFile(paths, blockId), 'utf-8')
}

/** Load a product_configuration by id. */
export async function loadProduct(paths: CatalogPaths, productId: string): Promise<Product> {
  const file = path.join(productsDir(paths), productId, 'product.yml')
  const text = await fs.readFile(file, 'utf-8')
  return parseYaml(text) as Product
}

/** Read raw product.yml text. */
export async function loadProductText(paths: CatalogPaths, productId: string): Promise<string> {
  const file = path.join(productsDir(paths), productId, 'product.yml')
  return fs.readFile(file, 'utf-8')
}

/** Parse inline product.yml text (no disk access). */
export function parseProductYaml(yamlText: string): Product {
  return parseYaml(yamlText) as Product
}

/* ── board.yaml ───────────────────────────────────────────────────────────
 *
 * The board is a property of a product INSTANCE (see BoardFile), so it is a
 * separate document handed alongside the product — a host reads the user's
 * one and passes it in.
 *
 * The catalog carries board files only for the two SAMPLE products that exist
 * to exercise this path in CI (`mosaico-bmgr-probe`, `c6-devkit-board-relay`).
 * Those live beside their product.yml, which is why the loader can find one by
 * product id at all. It is a fixture home, not a product key: the other 76
 * products have no board file and must never grow one. */

/** The board document's filename, beside product.yml for a sample product. */
export const BOARD_FILE = 'board.yaml'

/** Parse board.yaml text, refusing a shape we would otherwise half-honour.
 *
 *  Strict on purpose. A board file missing `selected` or `chip`, or carrying a
 *  `resolved_from` word nobody implements, is the false-pass shape this whole
 *  area keeps producing: generation quietly falls back to the boardless path
 *  and prints "✓ build succeeded" for a tree with no board in it. */
export function parseBoardYaml(text: string, where = 'board.yaml'): BoardFile {
  const doc = parseYaml(text) as Record<string, unknown> | null
  if (!doc || typeof doc !== 'object') {
    throw new Error(`${where}: not a YAML mapping`)
  }
  if (doc.schema !== undefined && doc.schema !== BOARD_SCHEMA) {
    throw new Error(`${where}: schema must be '${BOARD_SCHEMA}' (got ${JSON.stringify(doc.schema)})`)
  }
  if (typeof doc.selected !== 'string' || doc.selected.length === 0) {
    throw new Error(
      `${where}: 'selected' is required — the board the user picked, as an esp-virtual-parts board id. ` +
      `Hardware is chosen even when it is a bare module.`,
    )
  }
  if (typeof doc.chip !== 'string' || doc.chip.length === 0) {
    throw new Error(`${where}: 'chip' is required (e.g. esp32c6)`)
  }
  const board: BoardFile = { selected: doc.selected, chip: doc.chip }
  if (doc.schema !== undefined) board.schema = doc.schema as string
  if (doc.bmgr !== undefined && doc.bmgr !== null) {
    const b = doc.bmgr as Record<string, unknown>
    if (typeof b !== 'object' || Array.isArray(b)) throw new Error(`${where}: 'bmgr' must be a mapping`)
    if (!(b.board === null || typeof b.board === 'string')) {
      throw new Error(
        `${where}: bmgr.board must be a board name or null — null means no definition exists yet, ` +
        `which is a fact about our packs, not about the user's hardware.`,
      )
    }
    if (!BOARD_RESOLUTIONS.includes(b.resolved_from as never)) {
      throw new Error(
        `${where}: bmgr.resolved_from must be one of ${BOARD_RESOLUTIONS.join(' | ')} ` +
        `(got ${JSON.stringify(b.resolved_from)})`,
      )
    }
    board.bmgr = {
      board: (b.board as string | null) || null,
      resolved_from: b.resolved_from as BoardResolution,
    }
  }
  return board
}

/** The board file beside a catalog product's product.yml, or null when the
 *  product has none — which is every product but the two board samples. */
export async function loadBoardFile(
  paths: CatalogPaths,
  productId: string,
): Promise<BoardFile | null> {
  const file = path.join(productsDir(paths), productId, BOARD_FILE)
  let text: string
  try {
    text = await fs.readFile(file, 'utf-8')
  } catch {
    return null
  }
  return parseBoardYaml(text, file)
}

/** Walk every block.yml under code_blocks/ and return id + content. */
export async function listAllBlocks(
  paths: CatalogPaths,
): Promise<Array<{ id: string; kind: BlockKind; block: Block }>> {
  const root = blocksDir(paths)
  const out: Array<{ id: string; kind: BlockKind; block: Block }> = []
  let kindDirs: string[]
  try {
    kindDirs = await fs.readdir(root)
  } catch {
    return out
  }
  for (const kindDirName of kindDirs) {
    const kind = DIR_TO_BLOCK_KIND[kindDirName]
    if (!kind) continue
    const kindPath = path.join(root, kindDirName)
    let names: string[]
    try {
      names = await fs.readdir(kindPath)
    } catch {
      continue
    }
    for (const name of names) {
      const file = path.join(kindPath, name, 'block.yml')
      try {
        const text = await fs.readFile(file, 'utf-8')
        const block = parseYaml(text) as Block
        out.push({ id: `${kindDirName}/${name}`, kind, block })
      } catch {
        // ignore non-block entries
      }
    }
  }
  // Product-local blocks overlay the catalog under `local/<name>` ids. Kind
  // comes from the block itself (the dir name carries none).
  if (paths.localBlocksDir) {
    let names: string[] = []
    try {
      names = await fs.readdir(paths.localBlocksDir)
    } catch {
      // no local blocks — fine
    }
    for (const name of names) {
      const file = path.join(paths.localBlocksDir, name, 'block.yml')
      try {
        const text = await fs.readFile(file, 'utf-8')
        const block = parseYaml(text) as Block
        // Skip entries whose declared kind isn't a known block kind.
        if (!block.kind || !(block.kind in BLOCK_KIND_DIRS)) continue
        out.push({ id: `${LOCAL_BLOCK_PREFIX}${name}`, kind: block.kind, block })
      } catch {
        // ignore non-block entries
      }
    }
  }
  return out
}

/** Walk every product.yml under product_configurations/ and return id + content. */
export async function listAllProducts(
  paths: CatalogPaths,
): Promise<Array<{ id: string; product: Product }>> {
  const root = productsDir(paths)
  const out: Array<{ id: string; product: Product }> = []
  let ids: string[]
  try {
    ids = await fs.readdir(root)
  } catch {
    return out
  }
  for (const id of ids) {
    const file = path.join(root, id, 'product.yml')
    try {
      const text = await fs.readFile(file, 'utf-8')
      const product = parseYaml(text) as Product
      out.push({ id, product })
    } catch {
      // ignore non-product entries
    }
  }
  return out
}
