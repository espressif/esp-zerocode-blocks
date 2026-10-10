// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * The composition a product's first build starts from, before its own blocks
 * exist: the base firmware, the selected frameworks, one neutral instance (a
 * product needs at least one), and whatever each framework declares under
 * `early_build:`. Built from block data only.
 */
import { loadBlock } from './loader.js'
import { loadBoardFacts, type GeneratorPaths } from './generator.js'
import { bmgrBoardOf, type BoardFile, type Instance, type Product } from './types.js'

const NEUTRAL: Instance = { block: 'behaviors/uptime_log', prefix: 'EARLY_UPTIME', cfg: {} }
const MIB = 1024 * 1024

export type EarlyBuildComposition = { product: Product } | { skip: string }

export async function earlyBuildComposition(
  paths: GeneratorPaths,
  input: { frameworks: string[]; chip: string; board?: BoardFile | null },
): Promise<EarlyBuildComposition> {
  const bmgrBoard = bmgrBoardOf(input.board)
  const facts = bmgrBoard ? await loadBoardFacts(paths, bmgrBoard) : null
  const boardTypes = new Set((facts?.devices ?? []).map((d) => d.type))
  const instances: Instance[] = [NEUTRAL]
  let partitionTable: string | undefined

  for (const fw of [...input.frameworks].sort()) {
    const decl = (await loadBlock(paths, `frameworks/${fw}`)).early_build
    if (!decl) continue
    if (decl.min_flash_mb && facts?.flashBytes && facts.flashBytes < decl.min_flash_mb * MIB) {
      return { skip: `frameworks/${fw} needs ${decl.min_flash_mb} MB of flash; the board has ${facts.flashBytes / MIB} MB` }
    }
    if (decl.partition_table) {
      if (partitionTable && partitionTable !== decl.partition_table) {
        return { skip: `frameworks need different partition tables (${partitionTable}, ${decl.partition_table})` }
      }
      partitionTable = decl.partition_table
    }
    for (const inst of decl.instances ?? []) {
      if (inst.unless_board_has && boardTypes.has(inst.unless_board_has)) continue
      if (instances.some((i) => i.prefix === inst.prefix)) continue
      instances.push({ block: inst.block, prefix: inst.prefix, cfg: { ...(inst.cfg ?? {}) } })
    }
  }

  return {
    product: {
      id: 'early-build',
      name: 'Early build',
      description: 'The selected frameworks before the product adds its own blocks.',
      keywords: [],
      frameworks: [...input.frameworks],
      instances,
      ...(partitionTable ? { partition_table: partitionTable } : {}),
    },
  }
}
