// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Block-composition firmware generator.
 *
 * Takes a Product (loaded from product.yml or parsed inline), composes its
 * code_block instances, and writes a buildable ESP-IDF firmware tree to
 * `outDir`. This is the core of `scripts/generate-template.ts` factored out
 * so the agent's `apply_template` tool can call it directly.
 */

import { promises as fs } from 'node:fs'
import * as path from 'node:path'
import { parse as parseYaml, stringify as stringifyYaml } from 'yaml'
import {
  type Block,
  type BoardFile,
  type DeviceDataKey,
  type Product,
  type RenderedInstance,
  type ValueType,
  bmgrBoardOf,
  boardTakesOver,
} from './types.js'
import { loadBlock, loadBoardFile, type CatalogPaths } from './loader.js'
import { boardProvidesBus, boardsRoot, deviceRequirementMisses, findBoardDir, frameworkConflicts } from './validator.js'
import { CHIP_MODULES, normalizeChipTarget } from './hardware.js'
import { loadBaseDefaults, withDefaults } from './defaults.js'
import { DEVICE_DATA_FILE, type DeviceDataRow, collectDeviceData, deviceDataCsv } from './device-data.js'
import {
  collectParamIds,
  collectSlot,
  detectCatalogFeatures,
  indentText,
  isFramework,
  unwrapInitBody,
  VALUE_TYPE_TO_UNION_MEMBER,
} from './generator-common.js'
import { FRAMEWORK_GLUE } from './frameworks/index.js'
import type { FrameworkContext } from './frameworks/glue.js'
import { genAppMatterCpp } from './frameworks/matter.js'
import { genAppWebuiCpp } from './frameworks/webui.js'
import { genAppMqttCpp } from './frameworks/mqtt.js'
import { genAppAgentsCpp } from './frameworks/agents.js'
import { genAppAudioCpp } from './frameworks/audio.js'

// Kept on this module's public surface: the platform and the tests import them from here.
export { type CatalogFeatures, CURRENT_CATALOG, detectCatalogFeatures, wifiOwner } from './generator-common.js'
export { genAppAgentsCpp, genAppAudioCpp, genAppMatterCpp, genAppMqttCpp, genAppWebuiCpp }

const VALUE_TYPE_TO_C_COMMENT: Record<ValueType, string> = {
  bool: 'bool: on/off',
  u8: 'uint8_t',
  i16: 'int16_t',
  u16: 'uint16_t',
  u32: 'uint32_t',
}

const COPY_EXCLUDE = new Set([
  'build',
  'managed_components',
  'templates',
  '_generated',
  'sdkconfig',
  'sdkconfig.old',
  'dependencies.lock',
  'node_modules',
  'services',
  'defaults.yml',
  'deprecated_components.yml',
])

export interface GeneratorPaths extends CatalogPaths {
  /** Absolute path to firmware/base_firmware/. */
  baseFirmwareDir: string
  /** Root holding the esp-board-manager board packs
   *  (`<boardsDir>/<pack>/<board>/board_info.yaml`). Optional: a host that
   *  cannot reach a pack still generates, it just skips the board's
   *  memory-fact gates. Falls back to `ZC_BOARDS_DIR`. */
  boardsDir?: string
}

export interface GenerateInput {
  product: Product
  outDir: string
  /** The hardware this INSTANCE runs on — a parsed board.yaml, or null for
   *  none. A separate input because the board is not a property of the catalog
   *  entry: the same product is a bare module for one person and a devkit for
   *  the next (see BoardFile).
   *
   *  OMITTED (undefined) means "look for a sample board file beside the
   *  product.yml in the catalog" — the two board sample products, and nothing
   *  else. `null` says there is no board and suppresses the lookup. A host
   *  that owns the user's board file passes it explicitly and never depends on
   *  the lookup. */
  board?: BoardFile | null
  /** The chip this tree is generated FOR. Required: the tree is single-chip —
   *  only this chip's sdkconfig block is merged into sdkconfig.defaults. Must be
   *  a CHIP_MODULES target; when a board is given, must agree with its `chip`. */
  chip: string
  /** The product's own device data keys, beside the ones its blocks declare —
   *  the platform passes those its spec declares. Written to device_data.csv
   *  under PRODUCT_DEVICE_DATA_NAMESPACE. */
  deviceData?: DeviceDataKey[]
}

export interface GenerateResult {
  productId: string
  outDir: string
  blocks: string[]
  /** The device data written to `device_data.csv` — the blocks' keys, then
   *  the product's own; empty when there are none and no file was written. */
  deviceData: DeviceDataRow[]
  /** Non-fatal, and meant to be read: an instance a `bmgr.requires` guard kept
   *  out of the board manager (it generated with its own init instead).
   *  Present only when there is one — whoever composed the product reads
   *  this result, and an empty list on every product is noise. */
  warnings?: string[]
  /** What composed blocks already DO when a param changes — each declared
   *  `reacts:` sentence with the params its slot dispatches on. Present only
   *  when there is one. */
  reactions?: Reaction[]
}

export interface Reaction {
  block: string
  prefix: string
  /** The slot family: a framework id (`ble_hid`), `driver`, … */
  on: string
  /** The params the slot's `case` labels dispatch on, as rendered. */
  params: string[]
  does: string
}

/** The `reacts:` key a dispatch slot belongs to: `ble_hid_driver_cb_cases` →
 *  `ble_hid`, `driver_apply_cases` → `driver`, `zigbee_action_cases` →
 *  `zigbee_action`. Null for a slot that is not a dispatch. */
export function reactsKeyOf(slot: string): string | null {
  if (slot === 'driver_apply_cases') return 'driver'
  const m = /^(.+?)(?:_driver_cb)?_cases$/.exec(slot)
  return m ? m[1]! : null
}

/** The declared reactions of the rendered instances, for the slot families
 *  this product actually consumes (`driver`, or a framework it selected). */
export function collectReactions(rendered: RenderedInstance[], frameworks: readonly string[]): Reaction[] {
  const out: Reaction[] = []
  for (const r of rendered) {
    const reacts = r.block.reacts
    if (!reacts) continue
    for (const [slot, text] of Object.entries(r.slots)) {
      const key = reactsKeyOf(slot)
      if (!key || !reacts[key] || !text.trim()) continue
      if (key !== 'driver' && !frameworks.some((f) => key === f || key.startsWith(`${f}_`))) continue
      // A slot dispatches with `case APP_DRIVER_PARAM_X:` or `param_id == APP_DRIVER_PARAM_X`.
      const params = [...new Set([...text.matchAll(/\bcase\s+(APP_DRIVER_PARAM_\w+)\s*:|\bparam_id\s*==\s*(APP_DRIVER_PARAM_\w+)/g)].map((m) => (m[1] ?? m[2])!))]
      const ctx = { cfg: r.cfg, prefix: r.prefix, prefix_lc: r.prefix.toLowerCase() }
      // Soft: an optional param the product left unset reads as its key name
      // ("volume_param, when set") rather than failing the whole generate.
      const does = reacts[key]!.replace(/\{\{\s*([^}]+?)\s*\}\}/g, (_m, expr: string) => {
        const v = lookupSoft(expr, ctx)
        return v === undefined || v === null || v === '' ? expr.trim().replace(/^cfg\./, '') : String(v)
      }).trim()
      out.push({ block: r.block.id, prefix: r.prefix, on: key, params, does })
    }
  }
  return out
}

/** A block with `target: <chip>` applies ONLY when generating for that chip; a
 *  block with no `target` is chip-agnostic and always applies. Any block kind
 *  may carry it — a driver, behavior, framework or sdkconfig fragment that is
 *  meaningful on one chip only. The chip base blocks
 *  (`sdkconfig-fragments/<chip>`) are the built-in users of this. */
function targetMatches(block: { target?: string }, chip: string): boolean {
  return !block.target || normalizeChipTarget(block.target) === chip
}

/** Public entry point: compose `product`'s blocks and write a firmware tree
 *  to `outDir`. Overwrites whatever is at `outDir`. */
export async function generate(paths: GeneratorPaths, input: GenerateInput): Promise<GenerateResult> {
  const { outDir } = input
  // A table named by its path (`partition-tables/2mb`, `.../2mb/partitions.csv`)
  // means the bundled table of that name: the path is what a product's author
  // sees in the tree, and resolving it a second time under partition-tables/
  // made every such product fail on a table that exists.
  const asGiven = input.product.partition_table
    ? { ...input.product, partition_table: normalizePartitionTableName(input.product.partition_table) }
    : { ...input.product }
  const product = withDefaults(
    asGiven,
    await loadFrameworkBlocks(paths, asGiven.frameworks ?? []),
    await loadBaseDefaults(paths.baseFirmwareDir),
  )

  // The board the user picked, and the bmgr board definition it resolves to.
  // `bmgrBoard` — not the board file — is what every step below keys off: a
  // board file whose `bmgr.board` is null (no definition for that hardware
  // yet) generates the chip-agnostic tree, unchanged.
  const board = input.board === undefined ? await loadBoardFile(paths, product.id) : input.board
  const bmgrBoard = bmgrBoardOf(board)

  // The tree is single-chip: only this chip's sdkconfig block is merged into
  // sdkconfig.defaults (see below). Resolve + validate it up front. A board,
  // when given, is the user's hardware pick and must name the same chip.
  // normalizeChipTarget is fuzzy by design (it tolerates agent-emitted strings
  // like "esp32c3 4mb"), so it would quietly fold junk like "esp32xx" down to
  // "esp32". Here the chip is a required, canonical input, so reject anything
  // that isn't a target verbatim (case- and dash-insensitive) — an unknown chip
  // must fail loudly, not silently generate for the wrong one.
  if (typeof input.chip !== 'string' || !input.chip) {
    throw new Error(`generate() needs a chip — expected one of [${Object.keys(CHIP_MODULES).join(', ')}]`)
  }
  const chip = normalizeChipTarget(input.chip)
  const chipStripped = input.chip.toLowerCase().replace(/[^a-z0-9]/g, '')
  if (!(chip in CHIP_MODULES) || chipStripped !== chip) {
    throw new Error(
      `unknown chip '${input.chip}' — expected one of [${Object.keys(CHIP_MODULES).join(', ')}]`,
    )
  }
  if (board && normalizeChipTarget(board.chip) !== chip) {
    throw new Error(
      `chip mismatch: generate() was asked for '${chip}' but board.yaml selects '${board.chip}'`,
    )
  }

  // What the board's own definition says — its memory keys and the devices it
  // carries. Read HERE, before anything renders, because both of those are
  // inputs to rendering now: the memory keys decide which product sdkconfig
  // lines survive, and the device list decides whether a block whose hardware
  // the board already has keeps its own init (see BmgrMapping.provided_by).
  // Null = no reachable pack, and every gate below then behaves as it did.
  // Framework pairs that boot into an abort or a dead radio (see
  // frameworkConflicts) — refused before anything is written.
  const conflicts = frameworkConflicts(product.frameworks ?? [])
  if (conflicts.length > 0) {
    throw new Error(`product '${product.id}': ${conflicts.join(' ')}`)
  }

  const boardFacts = bmgrBoard ? await loadBoardFacts(paths, bmgrBoard) : null

  const instanceRendered = await renderInstances(paths, product, bmgrBoard, boardFacts, chip)
  const frameworkRendered = await renderFrameworkBlocks(paths, product, chip)
  const rendered = [...instanceRendered, ...frameworkRendered]
  // provides_internet is a Wi-Fi station. On a chip with Thread and no Wi-Fi
  // (esp32h2) Matter runs over Thread and wifi_station compiles out, so
  // nothing reaches an internet service from this product.
  const chipFeatures = CHIP_MODULES[chip]?.features ?? []
  if (chipFeatures.includes('thread') && !chipFeatures.includes('wifi')) {
    const needers = [...new Set(rendered.filter(r => r.block.needs_internet).map(r => r.block.id))]
    if (needers.length > 0) {
      throw new Error(`product '${product.id}': ${needers.join(', ')} needs internet over Wi-Fi, and ${chip} has no Wi-Fi (Matter on it runs over Thread) — pick a chip with Wi-Fi`)
    }
  }
  // A board product carries one extra, synthetic contributor — see
  // boardRendered(). It is deliberately NOT spliced into `rendered`: it must
  // reach main's slots, main's PRIV_REQUIRES, the merged idf_component.yml and
  // — when the board carries a panel — app_display.cpp, and nothing else, so
  // that a product with no board definition and one with it differ only there.
  const boardExtras: RenderedInstance[] = bmgrBoard ? [boardRendered(bmgrBoard, boardFacts)] : []

  const deviceData = collectDeviceData(rendered.map(r => ({ id: r.block.id, block: r.block })), input.deviceData)
  if (deviceData.errors.length > 0) {
    throw new Error(`product '${product.id}' has invalid device data declarations:\n  - ${deviceData.errors.join('\n  - ')}`)
  }

  await fs.rm(outDir, { recursive: true, force: true })
  await copyTree(paths.baseFirmwareDir, outDir)
  const features = await detectCatalogFeatures(paths.baseFirmwareDir)

  for (const r of frameworkRendered) {
    const blockComponentsDir = path.join(paths.templatesDir, 'code_blocks', r.block.id, 'components')
    if (await fileExists(blockComponentsDir)) {
      await copyTree(blockComponentsDir, path.join(outDir, 'components'))
    }
  }

  for (const r of frameworkRendered) {
    const name = r.block.id.split('/').pop() as string
    for (const hook of FRAMEWORK_CMAKE_HOOKS) {
      const src = path.join(paths.templatesDir, 'code_blocks', r.block.id, 'cmake', hook)
      if (await fileExists(src)) {
        await writeFileMk(path.join(outDir, 'cmake', 'frameworks', name, hook), await fs.readFile(src, 'utf8'))
      }
    }
  }

  await writeFileMk(
    path.join(outDir, 'components/app_driver/include/app_driver_types.h'),
    genAppDriverTypesH(rendered, input.product.params ?? {}),
  )
  await writeFileMk(
    path.join(outDir, 'components/app_config/include/app_config.h'),
    genAppConfigH(product, rendered),
  )
  await writeFileMk(
    path.join(outDir, 'components/app_driver/app_driver.cpp'),
    genAppDriverCpp(rendered),
  )
  const hasDeviceTypes = rendered.some(r => r.block.kind === 'device_type')
  const glueCtx: FrameworkContext = {
    rendered, features, hasDeviceTypes, product,
    productParams: input.product.params ?? {},
    boardExtras, bmgrBoard, boardFacts,
  }
  for (const fw of FRAMEWORK_GLUE) {
    if (!rendered.some(r => isFramework(r, fw.id))) continue
    for (const f of fw.files(glueCtx)) await writeFileMk(path.join(outDir, f.path), f.content)
  }
  const appLogic = genAppLogic(rendered)
  await writeFileMk(
    path.join(outDir, 'components/app_logic/app_logic.cpp'),
    appLogic.orchestrator,
  )
  for (const part of appLogic.parts) {
    await writeFileMk(path.join(outDir, 'components/app_logic', part.name), part.content)
  }
  if (appLogic.buttonRegistryHeader) {
    await writeFileMk(
      path.join(outDir, 'components/app_logic/include/app_button_registry.h'),
      appLogic.buttonRegistryHeader,
    )
  }
  // app_logic's CMakeLists globs every .c/.cpp in its directory, so the
  // per-behavior files (and any file the product adds) need no SRCS edit.
  // An older base lists its sources instead (SRCS "app_logic.cpp"): the
  // per-behavior files are added to that list, or they are never compiled.
  // A base without one (minimal test fixtures) gets the same glob.
  const logicCmakePath = path.join(outDir, 'components/app_logic/CMakeLists.txt')
  if (await fileExists(logicCmakePath)) {
    const listed = /SRCS\s+"app_logic\.cpp"/
    const logicCmake = await fs.readFile(logicCmakePath, 'utf-8')
    if (listed.test(logicCmake)) {
      const srcs = ['app_logic.cpp', ...appLogic.parts.map(p => p.name)].map(s => `"${s}"`).join(' ')
      await writeFileMk(logicCmakePath, logicCmake.replace(listed, `SRCS ${srcs}`))
    }
  } else {
    await writeFileMk(logicCmakePath, `${componentSrcGlob('app_logic_srcs')}${PRODUCT_CMAKE_HOOK}idf_component_register(\n    SRCS \${app_logic_srcs}\n    INCLUDE_DIRS "include"\n    PRIV_REQUIRES app_driver app_config app_utils nvs_flash esp_timer espressif__button driver esp_driver_gpio esp_driver_ledc esp_driver_i2s esp_driver_spi esp_driver_pcnt esp_driver_rmt esp_driver_uart esp_driver_gptimer \${PRODUCT_PRIV_REQUIRES}\n)\n`)
  }

  const mainPath = path.join(outDir, 'main/app_main.cpp')
  let appMain = await fs.readFile(mainPath, 'utf-8')
  // The board's INCLUDE goes first in main_includes; its INIT is not in
  // main_init at all — it renders into {{board_init}}, which sits above
  // app_driver_init(). See boardRendered().
  const mainRendered = [...boardExtras, ...rendered]
  appMain = appMain.replace('{{main_includes}}', collectSlot(mainRendered, 'main_includes'))
  appMain = appMain.replace('{{main_init}}', collectSlot(mainRendered, 'main_init', 4))
  const boardInit = collectSlot(boardExtras, 'board_init', 4)
  appMain = spliceLine(appMain, 'board_init', boardInit === '' ? '' : `${boardInit}\n`)
  appMain = spliceLine(
    appMain,
    'board_note',
    bmgrBoard
      ? ` * 1b. Board manager  → brings up the '${bmgrBoard}' board's on-board\n` +
        ` *${' '.repeat(22)}hardware, BEFORE the drivers that may ask it for a handle`
      : '',
  )
  await fs.writeFile(mainPath, appMain)

  await applyCmakePrivRequires(outDir, [...rendered, ...boardExtras])

  // Record the resolved bmgr board where the BUILD step can read it without
  // re-parsing anything. The build runs against the generated tree alone (the
  // platform hands an agent that tree and nothing else — neither product.yml
  // nor board.yaml is copied into it), so a one-line marker file in the output
  // is the only place both the generator and the build script can see. Kept
  // out of any CMake/README that a person or an agent might rewrite: a build
  // step that reads its board name out of prose is a build step that breaks on
  // an edit.
  if (bmgrBoard) {
    await fs.writeFile(path.join(outDir, '.zc-board'), `${bmgrBoard}\n`)
  }

  // The externally wired parts, as an amend on the board — see buildBoardAmend.
  // Written only when some instance actually declares a mapping; the build
  // step keys `-a` off the manifest's existence, so a board product with no
  // external parts runs the exact `idf.py bmgr` command it ran before.
  const amend = buildBoardAmend(product, rendered, bmgrBoard, boardFacts?.dir ?? null)
  if (amend) {
    const dir = path.join(outDir, AMEND_DIR)
    await fs.mkdir(dir, { recursive: true })
    await fs.writeFile(path.join(dir, AMEND_MANIFEST), amend.manifest)
    await fs.writeFile(path.join(dir, AMEND_FRAGMENT), amend.fragment)
  }

  // FLASH-BUDGET GATES (audio). The framework matrix can't see board flash
  // size, so the engine enforces it here where the partition table is known:
  //  - audio needs a `model` partition — a plain table BUILDS and then fails
  //    at boot with "no esp-sr models found" (the classic storage pitfall).
  //  - audio + a big transport doesn't fit 8mb-voice's 2.25MB OTA slots
  //    (MEASURED: minimal Matter+esp-sr = 2,313,680B in 2,359,296B — 98%);
  //    16mb-voice (3.5MB slots) is the deliberate path for 16MB hardware.
  const VOICE_TABLES = ['8mb-voice', '16mb-voice']
  if (product.frameworks.includes('audio')) {
    if (!VOICE_TABLES.includes(product.partition_table ?? '')) {
      throw new Error(
        `product '${product.id}' uses the audio framework but partition_table is ` +
        `'${product.partition_table ?? '(default)'}' — voice models need a 'model' partition. ` +
        `Set partition_table: 8mb-voice (or 16mb-voice with a Wi-Fi transport).`,
      )
    }
    const bigTransports = ['matter', 'rainmaker', 'ble_mesh'].filter(f => product.frameworks.includes(f))
    if (bigTransports.length > 0 && product.partition_table !== '16mb-voice') {
      throw new Error(
        `product '${product.id}' combines audio with ${bigTransports.join('+')} — that needs ` +
        `16MB flash (audio+${bigTransports[0]} fills 98% of 8mb-voice's OTA slot, measured). ` +
        `Set partition_table: 16mb-voice (requires a 16MB board).`,
      )
    }
    //  - NO per-chip rule: audio alone is ~1.35 MB on the esp32s3, esp32p4 and
    //    esp32s31 alike (measured 2026-09-09, ESP-IDF 6.2, esp-sr 2.4.7). The
    //    "S31 overflows by 268 KB" / "P4 by 887 KB" readings of Aug–Sep 2026
    //    were esp-sr 2.5's esp-dl dependency (860 KB of code the framework
    //    never calls), not the chips — the audio block pins 2.4.x for that
    //    reason. Re-measure before adding a chip rule here.
  }

  // THE BOARD OWNS MEMORY FACTS; THE PRODUCT OWNS THE PARTITION TABLE.
  //
  // `board_manager.defaults` is prepended to SDKCONFIG_DEFAULTS by `idf.py
  // bmgr` and therefore loads LAST-ish in a way the product cannot outrank on
  // the keys it sets — flash size, flash mode/freq, CPU frequency, PSRAM. That
  // is correct: those are facts about a physical part, not preferences. It is
  // also silent, and silence is the problem: an 8mb table on a board asserting
  // FLASHSIZE_16MB used to generate happily, and the flash-budget gates above
  // reasoned about a number the board contradicts.
  //
  // So when a board is present the engine reads the BOARD's own
  // sdkconfig.defaults.board and (a) refuses a partition table that does not
  // fit the board's flash, naming both, and (b) drops the product-side keys
  // the board already decides, so the generated tree states each memory fact
  // once instead of twice with a load-order tiebreak.
  if (boardFacts?.flashBytes) {
    const csvPath = product.partition_table
      ? path.join(paths.templatesDir, 'code_blocks', 'partition-tables', product.partition_table, 'partitions.csv')
      : path.join(paths.baseFirmwareDir, 'partitions.csv')
    let csv = ''
    try { csv = await fs.readFile(csvPath, 'utf-8') } catch { /* copy below reports a missing table */ }
    const needed = csv ? partitionTableEnd(csv) : 0
    // No table named and the default one does not fit: the board's flash decides,
    // so take the largest bundled table that fits rather than refusing a choice
    // the product never made. A table the product NAMED is still refused below.
    const named = Boolean(product.partition_table)
    if (needed > boardFacts.flashBytes && !named) {
      const fit = await largestFittingTable(paths, boardFacts.flashBytes)
      if (fit) product.partition_table = fit
    }
    if (needed > boardFacts.flashBytes && (named || !product.partition_table)) {
      throw new Error(
        `product '${product.id}' sets partition_table '${product.partition_table ?? '(default)'}', ` +
        `which needs ${mib(needed)} of flash, but board '${bmgrBoard}' has ` +
        `${mib(boardFacts.flashBytes)} (${boardFacts.flashSymbol} in ` +
        `${path.join(boardFacts.dir, BOARD_SDKCONFIG)}). The board owns the flash size; ` +
        `pick a partition table that fits it, or a board with more flash.`,
      )
    }
  }

  // The partition-table BLOCK owns the flash-size sdkconfig that matches its
  // layout (e.g. 16mb-voice carries CONFIG_ESPTOOLPY_FLASHSIZE_16MB) — the
  // table and the flash size must never drift apart. Appended alongside the
  // framework keys below (and per-chip, where choice symbols are decided).
  const partitionSdkconfig: Record<string, string> = {}
  if (product.partition_table) {
    const ptSrc = path.join(
      paths.templatesDir,
      'code_blocks',
      'partition-tables',
      product.partition_table,
      'partitions.csv',
    )
    const ptDst = path.join(outDir, 'partitions.csv')
    try {
      await fs.copyFile(ptSrc, ptDst)
    } catch (e) {
      const known = await bundledPartitionTables(paths)
      throw new Error(
        `partition_table '${product.partition_table}' is not a bundled table` +
        (known.length ? ` — use one of: ${known.join(', ')}` : '') + ` (looked for ${ptSrc}: ${(e as Error).message})`,
      )
    }
    try {
      const ptBlock = await loadBlock(paths, `partition-tables/${product.partition_table}`)
      for (const [k, v] of Object.entries(ptBlock.sdkconfig ?? {})) partitionSdkconfig[k] = String(v)
    } catch { /* a bare partitions.csv with no block.yml is fine */ }
  }

  if (deviceData.rows.length > 0) {
    await fs.writeFile(path.join(outDir, DEVICE_DATA_FILE), deviceDataCsv(deviceData.rows))
  }

  // ── sdkconfig.defaults ──────────────────────────────────────────────────
  // One merged file for the SELECTED chip (the tree is single-chip). IDF takes
  // the last assignment, so file order is precedence, low → high:
  //   1. the common base (base_firmware/sdkconfig.defaults, already copied) — a
  //      floor for keys nothing else sets;
  //   2. the chip block's keys (code_blocks/sdkconfig-fragments/<chip>) — beat
  //      the common base;
  //   3. framework, partition, instance-block, fragment and extra_sdkconfig
  //      contributions — appended last so they win Kconfig *choice* symbols
  //      (spelled as different key names, invisible to a key filter).
  // A contribution yields to a key the chip block itself sets (the chip base
  // stays authoritative for its own keys) UNLESS it is a product-level key (a
  // fragment or extra_sdkconfig the product asked for on purpose), which always
  // wins. Board-owned keys are stripped from contributions so the board's own
  // sdkconfig.defaults.board decides them; the chip base is left untouched so a
  // board's larger flash still overrides the chip's 4MB default.
  const sdkconfigPath = path.join(outDir, 'sdkconfig.defaults')

  // Framework blocks own their sdkconfig; the partition-table block owns the
  // matching flash-size key.
  const protoDefaults: Record<string, string> = {}
  for (const r of frameworkRendered) {
    for (const [k, v] of Object.entries(r.block.sdkconfig ?? {})) {
      protoDefaults[k] = String(v)
    }
  }
  Object.assign(protoDefaults, partitionSdkconfig)

  const blockSdkconfig: Record<string, string> = {}
  for (const r of instanceRendered) {
    for (const [k, v] of Object.entries(r.block.sdkconfig ?? {})) blockSdkconfig[k] = String(v)
  }

  // The selected chip's per-chip base, from its catalog block. A chip block is
  // OPTIONAL — one exists only for a chip with silicon-specific sdkconfig
  // (thread default, hosted radio). A chip whose config is fully covered by the
  // common base and the frameworks (esp32/c3/s3) has none, and that is correct,
  // not a chip-less tree: the chip itself is already validated against
  // CHIP_MODULES above.
  const chipBlockFile = path.join(paths.templatesDir, 'code_blocks', 'sdkconfig-fragments', chip, 'block.yml')
  const chipDefaults = (await fileExists(chipBlockFile))
    ? asStringMap((await loadBlock(paths, `sdkconfig-fragments/${chip}`)).sdkconfig ?? {})
    : {}
  const chipKeys = new Set(Object.keys(chipDefaults))
  if (chipKeys.size > 0) {
    const lines = Object.entries(chipDefaults).map(([k, v]) => `${k}=${v}`)
    await fs.appendFile(sdkconfigPath, `\n# Chip: ${chip}\n${lines.join('\n')}\n`)
  }

  // Product-level keys: fragments first, then extra_sdkconfig, so an explicit
  // per-product key beats a fragment it also selected. A fragment carrying
  // `target:` is chip-scoped — it contributes only when building for that chip
  // and is skipped otherwise (see targetMatches).
  const productContrib: Record<string, string> = {}
  for (const fragName of product.sdkconfig_fragments ?? []) {
    let block: Block
    try {
      block = await loadBlock(paths, `sdkconfig-fragments/${fragName}`)
    } catch (e) {
      throw new Error(`sdkconfig_fragment '${fragName}' not found: ${(e as Error).message}`)
    }
    if (!targetMatches(block, chip)) continue
    for (const [k, v] of Object.entries(block.sdkconfig ?? {})) productContrib[k] = String(v)
  }
  for (const [k, v] of Object.entries(product.extra_sdkconfig ?? {})) productContrib[k] = String(v)

  // framework/partition → instance blocks → product; product wins by insertion
  // order. Board-owned keys stripped. A key the chip base already sets is
  // dropped unless it is a product key (which the product stated on purpose).
  const contribs = { ...protoDefaults, ...blockSdkconfig, ...productContrib }
  dropBoardOwned(contribs, boardFacts)
  const productKeys = new Set(Object.keys(productContrib))
  const contribLines = Object.entries(contribs)
    .filter(([k]) => productKeys.has(k) || !chipKeys.has(k))
    .map(([k, v]) => `${k}=${v}`)
  if (contribLines.length > 0) {
    await fs.appendFile(
      sdkconfigPath,
      `\n# Block, framework and product contributions\n${contribLines.join('\n')}\n`,
    )
  }

  // The ESP32-P4 has no radio of its own; the base manifest pins the
  // ESP-Hosted stack (esp_wifi_remote + esp_hosted) for it so a networked
  // product keeps the esp_wifi API. A product whose frameworks are all local
  // (a screen, a speaker, a wake word — `radio: false` on every framework
  // block) has nothing to send over it, and the stack is not free: linked
  // whole-archive, it cost the voice product 278 KB of app (measured,
  // ESP-IDF 6.2). So it is dropped from the manifest unless some framework
  // needs a radio (a framework, or a block such as behaviors/wifi_station
  // that brings up Wi-Fi itself).
  const needsRadio = rendered.some(r => r.block.radio === true)
  const mergedYml = await mergeIdfComponents(
    path.join(paths.baseFirmwareDir, 'main/idf_component.yml'),
    [...rendered, ...boardExtras],
    chip === 'esp32p4' && !needsRadio ? P4_HOSTED_PACKAGES : [],
  )
  await fs.writeFile(path.join(outDir, 'main/idf_component.yml'), mergedYml)

  const warnings = rendered.filter(r => r.bmgrFallback).map(r => r.bmgrFallback!)
  const reactions = collectReactions(rendered, product.frameworks ?? [])
  return {
    productId: product.id,
    outDir,
    blocks: product.instances.map(i => i.block),
    deviceData: deviceData.rows,
    ...(warnings.length > 0 ? { warnings } : {}),
    ...(reactions.length > 0 ? { reactions } : {}),
  }
}

// ── rendering ──────────────────────────────────────────────────────────

function applyDefaults(instance: { cfg?: Record<string, unknown> }, block: Block): Record<string, unknown> {
  const cfg: Record<string, unknown> = { ...(instance.cfg ?? {}) }
  for (const [key, schema] of Object.entries(block.params ?? {})) {
    if (cfg[key] === undefined) {
      if (schema.default !== undefined) cfg[key] = schema.default
      else if (schema.required) throw new Error(`block ${block.id} requires param '${key}'`)
    }
    if (schema.enum && !schema.enum.includes(String(cfg[key]))) {
      throw new Error(`block ${block.id} param '${key}'='${cfg[key]}' not in [${schema.enum.join(', ')}]`)
    }
  }
  return cfg
}

/** Resolve a dotted path (e.g. `cfg.co2_ppm_param`) against `ctx`. Returns
 *  `undefined` when any segment is missing (callers decide whether that's an
 *  error — `substitute` throws, `lookupSoft` for conditionals treats it as
 *  falsy). */
function lookupSoft(expr: string, ctx: Record<string, unknown>): unknown {
  const segments = expr.trim().split('.')
  let cur: unknown = ctx
  for (const seg of segments) {
    if (cur && typeof cur === 'object' && seg in (cur as Record<string, unknown>)) {
      cur = (cur as Record<string, unknown>)[seg]
    } else {
      return undefined
    }
  }
  return cur
}

/** A value is "truthy" for `{{#if}}` when it's present and not one of the
 *  empty/false-y forms blocks use for "disabled" optional params: undefined,
 *  null, false, "", "0". (Optional params default to "" — see the block schemas
 *  with `default: ""` — so an empty string must read as off.) */
function isTruthy(v: unknown): boolean {
  if (v === undefined || v === null || v === false) return false
  if (typeof v === 'string') return v.trim() !== '' && v.trim() !== '0'
  if (typeof v === 'number') return v !== 0
  return true
}

/**
 * Fill a slot template. Supports two constructs:
 *   - `{{#if expr}} … {{/if}}` — emits the inner text only when `expr`
 *     resolves to a truthy/non-empty value (optional params default to "" and
 *     read as off). Conditionals may NOT nest. The inner text is still scanned
 *     for `{{var}}` substitutions.
 *   - `{{ dotted.path }}` — replaced with the resolved value; throws if the
 *     path is unresolved (kept strict so a typo'd placeholder fails loudly).
 * Conditionals are resolved first, then plain substitution runs on the result.
 */
function substitute(template: string, ctx: Record<string, unknown>): string {
  // Resolve {{#if expr}} … {{/if}} blocks. [\s\S] so the body can span lines;
  // non-greedy so adjacent independent blocks don't merge.
  const withConditionals = template.replace(
    /\{\{\s*#if\s+([^}]+?)\s*\}\}([\s\S]*?)\{\{\s*\/if\s*\}\}/g,
    (_m, expr: string, body: string) => (isTruthy(lookupSoft(expr, ctx)) ? body : ''),
  )
  return withConditionals.replace(/\{\{\s*([^}]+?)\s*\}\}/g, (_m, expr: string) => {
    const trimmed = expr.trim()
    // Guard: an unmatched #if / /if shouldn't be silently stringified.
    if (trimmed.startsWith('#if') || trimmed === '/if') {
      throw new Error(`unbalanced conditional: {{${trimmed}}}`)
    }
    const value = lookupSoft(trimmed, ctx)
    if (value === undefined) throw new Error(`unresolved placeholder: {{${trimmed}}}`)
    return String(value)
  })
}

export { substitute as __substituteForTest }

async function renderInstances(
  paths: CatalogPaths,
  product: Product,
  bmgrBoard: string | null,
  boardFacts: BoardFacts | null = null,
  chip = '',
): Promise<RenderedInstance[]> {
  const out: RenderedInstance[] = []
  for (const inst of product.instances) {
    const block = await loadBlock(paths, inst.block)
    // A block scoped to another chip contributes nothing here.
    if (!targetMatches(block, chip)) continue
    const cfg = applyDefaults(inst, block)
    const ctx = { cfg, prefix: inst.prefix, prefix_lc: inst.prefix.toLowerCase() }
    const slots: Record<string, string> = {}
    // ONE PIN, ONE OWNER. When there is a board definition AND this block's
    // `bmgr:` mapping says the board manager takes the hardware over, the
    // slots that mapping replaces are not rendered at all — otherwise the
    // amend's peripheral and the block's own driver_init both configure the
    // same line and which one wins is an ordering accident. No board, no
    // change: that is what keeps every general product byte-identical.
    //
    // `provided_by` narrows that to boards which ALREADY carry the hardware
    // (a soldered panel), where the block adds nothing and must step aside.
    // On a board without such a device the block keeps its own init: a panel
    // wired externally to a bare devkit is still the block's to drive, and
    // dropping its init there would cost the product its screen silently.
    // The `provided_by` branch is boardTakesOver() — ONE predicate shared
    // with the validator's pin-conflict exemption, so the two cannot diverge
    // again (they did: the generator stepped a display aside while the
    // validator reported its pins as an 11-line conflict wall).
    const providedBy = block.bmgr?.provided_by
    // A `bmgr.requires` guard that fails keeps the instance OUT of the board
    // manager: it generates exactly as it does on a board-less product, with
    // its own init, and the miss is reported in GenerateResult.warnings. It
    // used to refuse the whole composition, which pushed products toward
    // hand-wiring the part — the one outcome worse than either path.
    const guardMiss = bmgrBoard && block.bmgr?.init === 'board_manager'
      ? bmgrGuardMiss(block, cfg, `${block.id}${inst.prefix ? ` (prefix=${inst.prefix})` : ''}`, bmgrBoard)
      : null
    const boardOwns =
      !!bmgrBoard &&
      block.bmgr?.init === 'board_manager' &&
      !guardMiss &&
      (providedBy === undefined || boardTakesOver(block, boardFacts?.devices ?? []))
    // AN ADAPTER FOR HARDWARE THE BOARD ALREADY OWNS. `device_param` names the
    // cfg key holding a board DEVICE NAME; the block drives that device
    // through esp_board_manager and states no pin, because the pin is the
    // board's. Checked HERE rather than left to boot: the block generates and
    // links perfectly well against a device that does not exist, and says so
    // in one log line nobody reads. Same stance as the display adapter's "a
    // display product with no panel fails GENERATION", including telling the
    // two failures apart.
    const deviceParam = block.bmgr?.device_param
    if (deviceParam) {
      const want = String(cfg[deviceParam] ?? '')
      const type = providedBy ?? ''
      const where = `${inst.block} (prefix=${inst.prefix})`
      if (!bmgrBoard) {
        throw new Error(
          `${where} drives the board's own '${want}' device, but this product has no board ` +
          `(no board.yaml, or its bmgr.board is null). A block that binds a board device cannot ` +
          `run without one — wire the hardware and use a block that states its own pin instead.`)
      }
      if (!boardFacts) {
        throw new Error(
          `${where} wants board device '${want}' of type '${type}', but board '${bmgrBoard}'s ` +
          `pack could not be read, so it could not be checked (set ZC_BOARDS_DIR).`)
      }
      if (!boardFacts.devices.some(dv => dv.name === want && dv.type === type)) {
        const same = boardFacts.devices.filter(dv => dv.type === type).map(dv => `'${dv.name}'`)
        throw new Error(
          `${where} wants board device '${want}' of type '${type}', which board '${bmgrBoard}' ` +
          `does not declare. Its ${type} devices are: ${same.join(', ') || '(none)'}.`)
      }
      const req = block.bmgr?.device_requires
      const dev = boardFacts.devices.find(dv => dv.name === want && dv.type === type)!
      const misses = deviceRequirementMisses(dev, req)
      if (misses.length) {
        const ok = boardFacts.devices.filter(dv => dv.type === type && !deviceRequirementMisses(dv, req).length).map(dv => `'${dv.name}'`)
        throw new Error(
          `${where} needs its ${type} device to have ${Object.entries(req!).map(([k, v]) => `${k}: ${JSON.stringify(v)}`).join(', ')}, ` +
          `but board '${bmgrBoard}' declares '${want}' with ${misses.join(', ')}. ` +
          `Its devices that qualify: ${ok.join(', ') || '(none)'}.`)
      }
    }
    const replaced = new Set(boardOwns ? block.bmgr?.replaces_slots ?? [] : [])
    // An instance scoped to some frameworks gets no slot of the others: the
    // product's frameworks it did not name are dropped by slot family.
    const scoped = inst.frameworks
    if (scoped) {
      const unknown = scoped.filter((f) => !product.frameworks.includes(f))
      if (unknown.length) {
        throw new Error(`${inst.block} (prefix=${inst.prefix}) names frameworks: [${unknown.join(', ')}], which the product does not select (it selects [${product.frameworks.join(', ')}]).`)
      }
    }
    const outOfScope = scoped ? product.frameworks.filter((f) => !scoped.includes(f)) : []
    for (const [slot, body] of Object.entries(block.slots ?? {})) {
      if (replaced.has(slot)) continue
      if (outOfScope.some((f) => slot.startsWith(`${f}_`))) continue
      slots[slot] = substitute(body, ctx)
    }
    out.push({ block, prefix: inst.prefix, cfg, slots, replacedSlots: [...replaced], ...(guardMiss ? { bmgrFallback: guardMiss } : {}) })
  }
  return out
}

/** CMake that sets `var` to every .c/.cpp beside the CMakeLists, re-globbed
 *  at build time. Skipped in script mode, where IDF's requirement scan runs
 *  component CMakeLists and CONFIGURE_DEPENDS is an error. */
export function componentSrcGlob(variable: string): string {
  return `set(${variable})
if(NOT CMAKE_SCRIPT_MODE_FILE)
    file(GLOB ${variable} CONFIGURE_DEPENDS
         "\${CMAKE_CURRENT_LIST_DIR}/*.c" "\${CMAKE_CURRENT_LIST_DIR}/*.cpp")
endif()

`
}

/** app_logic's product-owned requirements: product.cmake beside its
 *  CMakeLists sets PRODUCT_PRIV_REQUIRES. The engine never writes that file.
 *  The glob re-runs CMake when it appears in a configured tree. */
const PRODUCT_CMAKE_HOOK = `set(PRODUCT_PRIV_REQUIRES)
if(NOT CMAKE_SCRIPT_MODE_FILE)
    file(GLOB product_cmake CONFIGURE_DEPENDS "\${CMAKE_CURRENT_LIST_DIR}/product.cmake")
endif()
include("\${CMAKE_CURRENT_LIST_DIR}/product.cmake" OPTIONAL)

`

/** CMake a framework block may ship under cmake/: included by the base
 *  CMakeLists before and after project(), only for selected frameworks. */
export const FRAMEWORK_CMAKE_HOOKS = ['pre_project.cmake', 'post_project.cmake'] as const

async function loadFrameworkBlocks(paths: CatalogPaths, names: string[]): Promise<Map<string, Block>> {
  const out = new Map<string, Block>()
  for (const name of names) out.set(name, await loadBlock(paths, `frameworks/${name}`))
  return out
}

async function renderFrameworkBlocks(paths: CatalogPaths, product: Product, chip = ''): Promise<RenderedInstance[]> {
  const out: RenderedInstance[] = []
  // frameworks is required; [] is a valid framework-less product (no blocks).
  for (const name of product.frameworks) {
    const block = await loadBlock(paths, `frameworks/${name}`)
    if (!targetMatches(block, chip)) continue
    const slots: Record<string, string> = { ...(block.slots ?? {}) }
    out.push({ block, prefix: '', cfg: {}, slots })
  }
  return out
}

/** Replace a marker that occupies a WHOLE LINE of a base_firmware template.
 *
 *  An empty replacement removes the line, marker and newline together, rather
 *  than leaving a blank one behind. That is what keeps a slot only some
 *  products contribute to free: a product that contributes nothing gets a file
 *  byte-identical to the one it got before the slot existed, so adding a
 *  conditional step to app_main.cpp is not a diff for every other product. */
function spliceLine(text: string, marker: string, replacement: string): string {
  const re = new RegExp(`^[ \\t]*\\{\\{${marker}\\}\\}\\n`, 'm')
  // Function form: a `$` in a board name or comment must not be read as a
  // replacement pattern.
  return text.replace(re, () => (replacement === '' ? '' : `${replacement}\n`))
}

// ── generated-file emitters ───────────────────────────────────────────

/** Solution-registry size for this product. Every framework registers one
 *  solution, and so does every instance of a block whose slot code calls
 *  app_driver_register_solution (nvs_persist_*, auto_off_timer, …). A fixed 8
 *  ran out on a busy product: the 9th registration logged an error and got
 *  handle 0 (= LOCAL), so its callback never ran — a persistence behavior
 *  silently stopped saving. Sized from the composition, with headroom for
 *  product code that registers its own; never below the old 8, so a product
 *  that fit keeps the same set_param stack footprint (one slot per entry is
 *  copied onto the caller's stack on every write). */
export function solutionRegistrySize(rendered: RenderedInstance[]): number {
  const registrants = rendered.filter(r =>
    r.block.kind === 'framework' ||
    Object.values(r.slots).some(code => code.includes('app_driver_register_solution(')),
  ).length
  return Math.max(8, registrants + 4)
}

function genAppDriverTypesH(rendered: RenderedInstance[], productParams: Record<string, string> = {}): string {
  const params = collectParamIds(rendered, productParams)
  const enumLines = [
    '    APP_DRIVER_PARAM_NONE = 0,  /* sentinel */',
    ...params.map(p =>
      `    ${p.name},  /* ${VALUE_TYPE_TO_C_COMMENT[p.type]} (union member: .${VALUE_TYPE_TO_UNION_MEMBER[p.type]}) */`,
    ),
  ].join('\n')
  return `/* Param IDs: each driver/endpoint declares which it references; their
 * union forms this enum. APP_DRIVER_PARAM_MAX is the count for the state array. */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
${enumLines}
    APP_DRIVER_PARAM_MAX
} app_driver_param_id_t;

typedef union {
    bool     b;
    uint8_t  u8;
    int16_t  i16;
    uint16_t u16;
    uint32_t u32;
} app_driver_param_val_t;

#define APP_DRIVER_MAX_SOLUTIONS ${solutionRegistrySize(rendered)}  /* registrants in this composition + headroom */
typedef uint8_t app_driver_handle_t;
#define APP_DRIVER_SOURCE_LOCAL 0

typedef void (*app_driver_notify_cb_t)(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx);
`
}

function genAppConfigH(product: Product, rendered: RenderedInstance[]): string {
  const meta = product.product_metadata ?? {}
  return `#pragma once

${collectSlot(rendered, 'config_defines')}

#define APP_PRODUCT_NAME        "${meta.product_name ?? product.name}"
#define APP_VENDOR_NAME         "${meta.vendor_name ?? 'ZeroCode AI'}"
`
}

function genAppDriverCpp(rendered: RenderedInstance[]): string {
  // Each driver's init becomes its own static function — isolating its locals
  // in function scope instead of a bare `{ }` block, and keeping app_driver_init
  // a flat, readable list of calls.
  const used = new Set<string>()
  const fns: string[] = []
  const calls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['driver_init'] ?? '').trim()) {
      // A driver whose init the board manager owns has NO init here by
      // design — say so, or the absence reads as a missing gpio_config().
      if ((r.replacedSlots ?? []).includes('driver_init')) {
        calls.push(`    /* ${r.block.id}${r.prefix ? ` (${r.prefix})` : ''}: pin brought up by the board manager` +
                   ` (esp_board_manager_init, before this runs) — nothing to do here */`)
      }
      continue
    }
    const slug = (r.prefix || r.block.id.split('/').pop() || 'drv').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; used.has(name); n++) name = `${slug}_${n}`
    used.add(name)
    fns.push(`static void zc_${name}_driver_init(void)\n{\n${indentText(unwrapInitBody(r.slots['driver_init']), 4)}\n}`)
    calls.push(`    zc_${name}_driver_init();`)
  }
  // driver_boot_state: an output driver writes the state it left its output
  // in onto its param, after EVERY init (its own driver_init, or the board
  // manager's, which replaces that slot) and before any framework registers,
  // so the param counts as written from boot instead of "never written" until
  // product code first switches it.
  const boot = collectSlot(rendered, 'driver_boot_state', 4)
  const bootState = boot ? `\n    /* Boot state of the outputs above, onto their params */\n${boot}\n` : '\n'
  return `#include "app_driver.h"
#include "app_config.h"

#include <esp_log.h>
${collectSlot(rendered, 'driver_includes')}

static const char *TAG = "app_driver";

${collectSlot(rendered, 'driver_statics')}

${fns.join('\n\n')}

esp_err_t app_driver_init(void)
{
${calls.join('\n')}
${bootState}
    ESP_LOGI(TAG, "Hardware drivers initialized");
    return ESP_OK;
}

esp_err_t app_driver_apply_param(app_driver_param_id_t id, app_driver_param_val_t val)
{
    switch (id) {
${collectSlot(rendered, 'driver_apply_cases', 8)}
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}
`
}

/** Deduped union of every behavior's logic_includes. Each per-behavior file
 *  gets them all, so a behavior that relied on a sibling's include in the old
 *  monolith still compiles after the split.
 *
 *  Dedup is per WHOLE SLOT, not per line: a block may wrap includes in
 *  \`#if SOC_X\` conditionals, and line-level dedup let a conditional copy of
 *  \`#include <esp_timer.h>\` swallow another block's unconditional one —
 *  which vanished entirely on chips where the condition is false (bit us:
 *  encoder's PCNT-guarded esp_timer.h ate console_uptime's on esp32c3).
 *  Duplicate #include lines across slots are free — headers have guards. */
function allLogicIncludes(rendered: RenderedInstance[]): string {
  // Dedup per LINE, first occurrence wins, order kept. Header guards make a
  // repeated #include harmless to the compiler, but three factory-reset
  // behaviors each bringing <nvs_flash.h> made every concern file open with
  // the same block three times over — noise that reads as a defect to anyone
  // (or any model) reviewing the tree. A non-#include line (a macro, a
  // forward declaration a behavior authored in its includes slot) is kept
  // verbatim and never merged with an identical one from another block.
  const seen = new Set<string>()
  const out: string[] = []
  for (const r of rendered) {
    const slot = (r.slots['logic_includes'] ?? '').trim()
    if (!slot) continue
    for (const line of slot.split('\n')) {
      const t = line.trim()
      if (t.startsWith('#include')) {
        if (seen.has(t)) continue
        seen.add(t)
      }
      out.push(line)
    }
  }
  return out.join('\n')
}

export interface AppLogicOutput {
  orchestrator: string
  parts: Array<{ name: string; content: string }>
  /** When any block creates an iot_button device, the shared button registry
   *  header that all button blocks `#include`. Written to
   *  components/app_logic/include/app_button_registry.h by the caller. */
  buttonRegistryHeader?: string
}

/** True when a rendered block creates/uses an iot_button device. Detected via
 *  its `espressif/button` IDF dependency, or any slot referencing iot_button —
 *  so the check stays general as new button-creating blocks are authored. */
function usesButton(r: RenderedInstance): boolean {
  if ('espressif/button' in (r.block.idf_components ?? {})) return true
  for (const body of Object.values(r.slots)) {
    if (/iot_button|button_gpio|app_button_get_or_create/.test(body)) return true
  }
  return false
}

/** The shared button registry header. All button-creating blocks `#include`
 *  this and call app_button_get_or_create() instead of making their own
 *  iot_button device, so a control button + long-press factory-reset on the
 *  SAME GPIO share a single device (esp_iot_button v4.x binds one device per
 *  GPIO; two devices on one pin race). Each consumer then registers its own
 *  event callback on the returned handle. */
const BUTTON_REGISTRY_HEADER = `#pragma once
/* Shared GPIO-button registry — keeps one iot_button device per GPIO so that
 * multiple consumers (e.g. a control button + long-press factory reset) on the
 * same pin share a single device and each register their own event callback. */
#include <iot_button.h>
#include <button_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Get the iot_button handle for gpio_num, creating the device on first use.
 * active_level and long_press_ms are applied only when the device is first
 * created; later callers for the same GPIO get the existing handle. Pass
 * long_press_ms = 0 to leave the device-level long-press at its default.
 * Returns NULL on failure. */
button_handle_t app_button_get_or_create(int gpio_num, int active_level, int long_press_ms);

/* The factory-reset hold armed on a registry button (behaviors/
 * factory_reset_long_press records it), so a control button on the same pin
 * can toggle on any press released before it instead of on the click alone.
 * app_button_reset_hold_ms() is 0 for a button with no reset hold. */
void app_button_set_reset_hold(button_handle_t handle, int hold_ms);
int app_button_reset_hold_ms(button_handle_t handle);

#ifdef __cplusplus
}
#endif
`

const BUTTON_REGISTRY_SOURCE = `#include "app_button_registry.h"
#include <esp_log.h>

static const char *TAG = "button_registry";

#define APP_BUTTON_REGISTRY_MAX 8

typedef struct {
    int gpio_num;
    button_handle_t handle;
    int reset_hold_ms;
} app_button_entry_t;

static app_button_entry_t s_buttons[APP_BUTTON_REGISTRY_MAX];
static int s_button_count = 0;

button_handle_t app_button_get_or_create(int gpio_num, int active_level, int long_press_ms)
{
    for (int i = 0; i < s_button_count; i++) {
        if (s_buttons[i].gpio_num == gpio_num) {
            return s_buttons[i].handle;
        }
    }
    if (s_button_count >= APP_BUTTON_REGISTRY_MAX) {
        ESP_LOGE(TAG, "button registry full (max %d) — GPIO %d not created", APP_BUTTON_REGISTRY_MAX, gpio_num);
        return NULL;
    }

    button_config_t btn_cfg = {};
    btn_cfg.long_press_time = (uint16_t)long_press_ms;
    button_gpio_config_t gpio_cfg = {
        .gpio_num = gpio_num,
        .active_level = (uint8_t)active_level,
        .enable_power_save = false,
        .disable_pull = false,
    };
    button_handle_t handle = NULL;
    esp_err_t err = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &handle);
    if (err != ESP_OK || !handle) {
        ESP_LOGE(TAG, "failed to create button on GPIO %d: %s", gpio_num, esp_err_to_name(err));
        return NULL;
    }
    s_buttons[s_button_count].gpio_num = gpio_num;
    s_buttons[s_button_count].handle = handle;
    s_buttons[s_button_count].reset_hold_ms = 0;
    s_button_count++;
    ESP_LOGI(TAG, "created shared button device on GPIO %d", gpio_num);
    return handle;
}

void app_button_set_reset_hold(button_handle_t handle, int hold_ms)
{
    for (int i = 0; i < s_button_count; i++) {
        if (s_buttons[i].handle == handle) {
            s_buttons[i].reset_hold_ms = hold_ms;
            return;
        }
    }
}

int app_button_reset_hold_ms(button_handle_t handle)
{
    for (int i = 0; i < s_button_count; i++) {
        if (s_buttons[i].handle == handle) return s_buttons[i].reset_hold_ms;
    }
    return 0;
}
`

/** Map a behavior to a logical app_logic file. Product-specific logic
 *  (endpoints/drivers) and anything uncategorized go to `control`. */
const LOGIC_CATEGORY_PATTERNS: Array<[RegExp, string]> = [
  [/^nvs_persist/, 'persist'],
  [/^factory_reset/, 'factory_reset'],
  [/^console_/, 'console'],
  [/^(identify_|blink_pattern$|commissioning_led$|status_led$)/, 'indicator'],
  [/^(auto_|level_change)/, 'automation'],
  [/^(boot_|uptime_log$|network_status_log$|wifi_disconnect_recovery$|task_watchdog$|param_log|sntp_time$)/, 'diagnostics'],
]

function logicCategory(block: Block): string {
  if (block.kind !== 'behavior') return 'control'
  const name = block.id.split('/').pop() ?? ''
  for (const [re, cat] of LOGIC_CATEGORY_PATTERNS) if (re.test(name)) return cat
  return 'control'
}

/** Split app_logic into a handful of clearly-named, single-concern files
 *  (persist / factory_reset / indicator / console / automation / diagnostics /
 *  control), plus a slim app_logic.cpp that calls each category's init. Within
 *  a category file each behavior is its own `static void <name>_init()` — so
 *  the agent edits a small focused file, and the bare `{ }` scope blocks are
 *  gone (each behavior's locals are now its function's scope). */
export function genAppLogic(rendered: RenderedInstance[]): AppLogicOutput {
  const includes = allLogicIncludes(rendered)
  // Group behaviors that emit app_logic code by category, preserving order.
  const groups = new Map<string, RenderedInstance[]>()
  for (const r of rendered) {
    if (!(r.slots['logic_statics'] ?? '').trim() && !(r.slots['logic_init'] ?? '').trim() && !(r.slots['logic_start'] ?? '').trim()) continue
    const cat = logicCategory(r.block)
    if (!groups.has(cat)) groups.set(cat, [])
    groups.get(cat)!.push(r)
  }
  const parts: Array<{ name: string; content: string }> = []
  const calls: string[] = []
  const decls: string[] = []
  const startCalls: string[] = []
  for (const [cat, members] of groups) {
    const lines = [
      `#include "app_logic.h"`,
      `#include "app_driver.h"`,
      `#include "app_config.h"`,
      `#include <esp_log.h>`,
    ]
    if (includes) lines.push(includes)
    lines.push('')
    const innerCalls: string[] = []
    const innerStarts: string[] = []
    const used = new Set<string>()
    let usesTag = false
    const bodyLines: string[] = []
    for (const r of members) {
      const statics = (r.slots['logic_statics'] ?? '').trim()
      const initRaw = (r.slots['logic_init'] ?? '').trim()
      const startRaw = (r.slots['logic_start'] ?? '').trim()
      usesTag = usesTag || /\bTAG\b/.test(`${statics} ${initRaw} ${startRaw}`)
      const slug = (r.prefix || r.block.id.split('/').pop() || 'b').toLowerCase().replace(/[^a-z0-9_]/g, '_')
      let name = slug
      for (let n = 2; used.has(name); n++) name = `${slug}_${n}`
      used.add(name)
      bodyLines.push(`/* ${r.block.id}${r.prefix ? ` (${r.prefix})` : ''} */`)
      if (statics) bodyLines.push(statics, '')
      if (initRaw) {
        bodyLines.push(`static void zc_${name}_init(void)`, '{', indentText(unwrapInitBody(r.slots['logic_init']), 4), '}', '')
        innerCalls.push(`    zc_${name}_init();`)
      }
      if (startRaw) {
        bodyLines.push(`static void zc_${name}_start(void)`, '{', indentText(unwrapInitBody(r.slots['logic_start']), 4), '}', '')
        innerStarts.push(`    zc_${name}_start();`)
      }
    }
    if (usesTag) lines.push(`static const char *TAG = "${cat}";`, '')
    lines.push(...bodyLines)
    lines.push(`void ${cat}_logic_init(void)`, '{', ...innerCalls, '}', '')
    calls.push(`    ${cat}_logic_init();`)
    decls.push(`void ${cat}_logic_init(void);`)
    if (innerStarts.length > 0) {
      lines.push(`void ${cat}_logic_start(void)`, '{', ...innerStarts, '}', '')
      startCalls.push(`    ${cat}_logic_start();`)
      decls.push(`void ${cat}_logic_start(void);`)
    }
    parts.push({ name: `${cat}.cpp`, content: lines.join('\n') })
  }
  // Shared button registry: when any block creates an iot_button device, emit
  // one extra part (gets the deduped logic includes + auto-added to SRCS like
  // every other part) so a control button + long-press factory reset on the
  // SAME GPIO share a single device instead of racing two.
  const needsButtonRegistry = rendered.some(usesButton)
  if (needsButtonRegistry) {
    parts.unshift({ name: 'button_registry.cpp', content: BUTTON_REGISTRY_SOURCE })
  }

  // The composed behaviors are wired in their OWN generated file, called by
  // app_main before app_logic_init(). app_logic.cpp is the product's file —
  // the one an agent or a person rewrites to add the product's logic — and a
  // whole-file rewrite of it used to drop the concern calls with it, turning
  // every composed behavior (console commands, persistence, factory reset,
  // diagnostics) into dead code with no build or boot symptom.
  parts.push({
    name: 'zc_behaviors.cpp',
    content: `/* GENERATED — the composed behaviors' entry point. Do not put product logic
 * here and do not edit: the generator rewrites this file. Product logic
 * belongs in app_logic.cpp (app_logic_init) or its own file. */
#include "app_logic.h"
#include <esp_log.h>

static const char *TAG = "zc_behaviors";

/* Each concern lives in its own translation unit — declared here, run below. */
${decls.join('\n')}

esp_err_t zc_behaviors_init(void)
{
${calls.join('\n')}

    ESP_LOGI(TAG, "Composed behaviors initialized");
    return ESP_OK;
}

/* After app_logic_init(): what the composed behaviors do once the product's
 * own logic is up and every saved value is restored (logic_start slots). */
esp_err_t zc_behaviors_start(void)
{
${startCalls.map(c => `${c}\n`).join('')}    return ESP_OK;
}
`,
  })

  const orchestrator = `#include "app_logic.h"
#include "app_driver.h"
#include "app_config.h"
#include <esp_log.h>

static const char *TAG = "app_logic";

/* Product logic: state machines, automation rules, timers — whatever the
 * product does beyond what its composed blocks already do. Runs after the
 * drivers, the frameworks and the composed behaviors (zc_behaviors.cpp) are
 * up, so every bus param is readable and every solution is registered. */
esp_err_t app_logic_init(void)
{
    ESP_LOGI(TAG, "App logic initialized");
    return ESP_OK;
}
`
  return {
    orchestrator,
    parts,
    buttonRegistryHeader: needsButtonRegistry ? BUTTON_REGISTRY_HEADER : undefined,
  }
}

// ── CMake + idf_component.yml weaving ─────────────────────────────────

async function applyCmakePrivRequires(outDir: string, rendered: RenderedInstance[]): Promise<void> {
  const extras = new Map<string, Set<string>>()
  for (const r of rendered) {
    for (const [component, requires] of Object.entries(r.block.cmake_priv_requires ?? {})) {
      if (!extras.has(component)) extras.set(component, new Set())
      const set = extras.get(component)!
      for (const dep of requires) set.add(dep)
    }
  }
  if (extras.size === 0) return

  for (const [component, set] of extras) {
    const cmakePath = component === 'main'
      ? path.join(outDir, 'main', 'CMakeLists.txt')
      : path.join(outDir, 'components', component, 'CMakeLists.txt')
    let body: string
    try {
      body = await fs.readFile(cmakePath, 'utf-8')
    } catch {
      continue
    }
    const extrasList = [...set].join(' ')
    const updated = body.replace(
      /^(\s*PRIV_REQUIRES\s+)([^\n)]*)/m,
      (_match, prefix: string, existing: string) => {
        const have = new Set(existing.trim().split(/\s+/).filter(Boolean))
        for (const e of set) have.add(e)
        return `${prefix}${[...have].join(' ')}`
      },
    )
    if (updated === body) {
      const inserted = body.replace(
        /idf_component_register\(([\s\S]*?)\)/,
        (m, inner: string) => `idf_component_register(${inner.trimEnd()}\n    PRIV_REQUIRES ${extrasList}\n)`,
      )
      await fs.writeFile(cmakePath, inserted)
    } else {
      await fs.writeFile(cmakePath, updated)
    }
  }
}

interface IdfComponentYml {
  dependencies: Record<string, string | { version?: string; [k: string]: unknown }>
}

/** esp-board-manager version pin for a `board:` product.
 *
 *  A range, not `"*"`: the board YAML schema, the `idf.py bmgr` action that
 *  reads it and the C the action generates are ONE artifact split across the
 *  component and the board packs. `"*"` would let a future major swap the
 *  generated code out from under board definitions written against 0.7 — and
 *  the failure would land at link time in generated sources nobody wrote.
 *  The floor follows the platform's pinned esp-board-manager checkout, which
 *  supplies the `bmgr` action: its generated C must compile against the
 *  component it links. 0.7.6's emits `init_level` on every gpio_ctrl device,
 *  a field 0.7.5 lacks. `~` keeps the two on one minor. */
const BOARD_MANAGER_VERSION = '~0.7.6'

/** The synthetic contributor a `board:` product gets: the dependency, main's
 *  include + init call, and main's PRIV_REQUIRES entry.
 *
 *  It is shaped as a Block so it rides the same three merge paths every real
 *  block uses rather than adding three special cases. It contributes NO other
 *  slot, so app_driver / app_logic / the framework components cannot see it.
 *
 *  `components/gen_bmgr_codes` (written by `idf.py bmgr`, not by us) is
 *  WHOLE_ARCHIVE and REQUIRES esp_board_manager, so the board's peripherals
 *  and devices are registered whether or not main calls init. The init call is
 *  here so the product actually BRINGS UP that hardware.
 *
 *  The init lands in `board_init`, NOT `main_init`. `main_init` renders after
 *  `app_driver_init()`, so a driver block that fetches a board handle from its
 *  `driver_init` slot would run against a board that does not exist yet — the
 *  whole point of collapsing the driver blocks onto the board (phase 3). The
 *  INCLUDE stays in `main_includes`; only the call moved. */
/** The board pack file that states a board's memory facts. */
const BOARD_SDKCONFIG = 'sdkconfig.defaults.board'

/** The board pack file that lists a board's on-board devices. */
const BOARD_DEVICES = 'board_devices.yaml'

/** One entry of a board's `board_devices.yaml`.
 *
 *  `name` is what `esp_board_manager_get_device_handle()` is called with, and
 *  `type` is what decides which handle struct comes back. They are usually the
 *  same word (bmgr's canonical names — `ESP_BOARD_DEVICE_NAME_DISPLAY_LCD` is
 *  literally "display_lcd") but they are not the same THING, so a board that
 *  names its panel `display_lcd_0` is matched on type and fetched by name. */
export interface BoardDevice {
  name: string
  type: string
  /** the part on it (`co5300`, `cst9217`, …), as the board declares it */
  chip?: string
  /** the device's `config:` mapping, as the board declares it */
  config?: Record<string, unknown>
}

/** What the engine reads out of a board pack at GENERATION time. */
export interface BoardFacts {
  /** The board's directory inside the pack. */
  dir: string
  /** Flash size in bytes, from CONFIG_ESPTOOLPY_FLASHSIZE_<n>MB. */
  flashBytes: number | null
  /** The symbol that supplied it, for error messages. */
  flashSymbol: string | null
  /** Every key the board's sdkconfig.defaults.board sets. */
  keys: Set<string>
  /** The devices the board definition declares, in file order. */
  devices: BoardDevice[]
}

/** The board's first device of `type`, or null. First rather than "the one":
 *  a board may carry two panels, and the display framework drives one screen —
 *  saying which one it picked (it logs the name) beats refusing to build. */
function boardDeviceOfType(facts: BoardFacts | null, type: string): BoardDevice | null {
  return facts?.devices.find(d => d.type === type) ?? null
}

/** Kconfig CHOICE families a board decides. A choice is spelled as a different
 *  KEY per value (…FLASHSIZE_8MB vs …FLASHSIZE_16MB), so an exact-key filter
 *  cannot see that the product and the board are arguing about one setting.
 *  If the board sets any key under one of these prefixes, every product key
 *  under it is dropped. */
const BOARD_CHOICE_PREFIXES = [
  'CONFIG_ESPTOOLPY_FLASHSIZE_',
  'CONFIG_ESPTOOLPY_FLASHMODE_',
  'CONFIG_ESPTOOLPY_FLASHFREQ_',
  'CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_',
  'CONFIG_SPIRAM_MODE_',
  'CONFIG_SPIRAM_SPEED_',
]

/** Read a board's memory facts out of its pack.
 *
 *  Pack RESOLUTION is the validator's (`boardsRoot` + `findBoardDir`), so
 *  there is ONE answer to "where do boards live" shared by the `board:`
 *  validation rules, this gate and `scripts/build-product.sh`'s
 *  `ZC_BOARDS_DIR`. A board pack is host content like ESP-IDF: not reachable
 *  means the gates below are SKIPPED, not failed, so a machine (or a Lambda)
 *  with no packs checked out generates exactly what it generated before. */
export async function loadBoardFacts(paths: GeneratorPaths, board: string): Promise<BoardFacts | null> {
  const dir = findBoardDir(boardsRoot({ boardsDir: paths.boardsDir }), board)
  if (!dir) return null
  let text = ''
  try { text = await fs.readFile(path.join(dir, BOARD_SDKCONFIG), 'utf-8') } catch { /* board sets none */ }
  const keys = new Set<string>()
  for (const raw of text.split('\n')) {
    const line = raw.trim()
    if (!line || line.startsWith('#')) continue
    const eq = line.indexOf('=')
    if (eq > 0) keys.add(line.slice(0, eq).trim())
  }
  let flashBytes: number | null = null
  let flashSymbol: string | null = null
  for (const k of keys) {
    const m = /^CONFIG_ESPTOOLPY_FLASHSIZE_(\d+)MB$/.exec(k)
    if (m) {
      flashBytes = Number(m[1]) * 1024 * 1024
      flashSymbol = `${k}=y`
    }
  }
  // The board's on-board devices. Read here rather than guessed from the
  // board's NAME: "does this board have a panel" is a fact stated in the board
  // definition, and a display adapter generated against a guess would be a
  // product that fetches a handle nothing ever registered.
  const devices: BoardDevice[] = []
  try {
    const doc = parseYaml(await fs.readFile(path.join(dir, BOARD_DEVICES), 'utf-8')) as
      { devices?: unknown } | null
    for (const d of Array.isArray(doc?.devices) ? doc.devices : []) {
      const e = d as Record<string, unknown>
      // A device may state its type via `type:` (display_lcd, lcd_touch) or,
      // for the simple ones, only via `chip:`/`sub_type:`. Only `type:` is
      // matched: it is the field bmgr itself dispatches on.
      if (typeof e?.name === 'string' && typeof e?.type === 'string') {
        const config = e.config && typeof e.config === 'object' ? e.config as Record<string, unknown> : undefined
        devices.push({
          name: e.name,
          type: e.type,
          ...(typeof e.chip === 'string' ? { chip: e.chip } : {}),
          ...(config ? { config } : {}),
        })
      }
    }
  } catch { /* a board may declare no devices at all */ }
  return { dir, flashBytes, flashSymbol, keys, devices }
}

/** Remove, in place, the keys a board already decides. No board (or no
 *  reachable pack) means no change at all — which is what keeps every
 *  boardless product's tree byte-identical. */
function dropBoardOwned(map: Record<string, string>, facts: BoardFacts | null): void {
  if (!facts) return
  const activePrefixes = BOARD_CHOICE_PREFIXES.filter(p => [...facts.keys].some(k => k.startsWith(p)))
  for (const k of Object.keys(map)) {
    if (facts.keys.has(k) || activePrefixes.some(p => k.startsWith(p))) delete map[k]
  }
}

function asStringMap(o: Record<string, unknown>): Record<string, string> {
  const out: Record<string, string> = {}
  for (const [k, v] of Object.entries(o)) out[k] = String(v)
  return out
}

/** Parse a partition-table size/offset cell: `0x20000`, `1M`, `24K`, `4096`. */
function parseCsvSize(cell: string): number | null {
  const s = cell.trim()
  if (!s) return null
  const m = /^(0x[0-9a-fA-F]+|\d+)([KkMm]?)$/.exec(s)
  if (!m) return null
  const n = m[1].startsWith('0x') ? parseInt(m[1], 16) : parseInt(m[1], 10)
  if (Number.isNaN(n)) return null
  if (m[2].toLowerCase() === 'k') return n * 1024
  if (m[2].toLowerCase() === 'm') return n * 1024 * 1024
  return n
}

/** The last byte a partition table needs — i.e. the smallest flash it fits in.
 *
 *  Offsets may be blank ("place me after the previous one"), which is why this
 *  walks a cursor rather than reading the final row: `nvs_keys`, `otadata` and
 *  `phy_init` are blank in every table here. Blank offsets are aligned the way
 *  gen_esp32part.py aligns them — 64 KB for app partitions, 4 KB for data. */
export function partitionTableEnd(csv: string): number {
  let cursor = 0
  let end = 0
  for (const raw of csv.split('\n')) {
    const line = raw.split('#')[0].trim()
    if (!line) continue
    const cols = line.split(',').map(c => c.trim())
    if (cols.length < 5) continue
    const size = parseCsvSize(cols[4])
    if (size === null) continue
    const align = cols[1] === 'app' ? 0x10000 : 0x1000
    const given = parseCsvSize(cols[3])
    const start = given ?? Math.ceil(cursor / align) * align
    cursor = start + size
    if (cursor > end) end = cursor
  }
  return end
}

/** `partition-tables/2mb`, `code_blocks/partition-tables/2mb/partitions.csv`
 *  and `2mb` all name the bundled table `2mb`. */
export function normalizePartitionTableName(name: string): string {
  return name.trim()
    .replace(/^(\.\/)+/, '')
    .replace(/^(code_blocks\/)?partition-tables\//, '')
    .replace(/\/partitions\.csv$/, '')
    .replace(/\/+$/, '')
}

/** The bundled partition tables (directories holding a partitions.csv), sorted. */
async function bundledPartitionTables(paths: GeneratorPaths): Promise<string[]> {
  const dir = path.join(paths.templatesDir, 'code_blocks', 'partition-tables')
  let names: string[] = []
  try { names = await fs.readdir(dir) } catch { return [] }
  const out: string[] = []
  for (const n of names.sort()) {
    try { await fs.access(path.join(dir, n, 'partitions.csv')); out.push(n) } catch { /* not a table */ }
  }
  return out
}

/** The bundled table with the most flash that still fits `flashBytes`, or null. */
async function largestFittingTable(paths: GeneratorPaths, flashBytes: number): Promise<string | null> {
  let best: { name: string; end: number } | null = null
  for (const name of await bundledPartitionTables(paths)) {
    let csv = ''
    try { csv = await fs.readFile(path.join(paths.templatesDir, 'code_blocks', 'partition-tables', name, 'partitions.csv'), 'utf-8') } catch { continue }
    const end = partitionTableEnd(csv)
    if (end > 0 && end <= flashBytes && (!best || end > best.end)) best = { name, end }
  }
  return best?.name ?? null
}

function mib(bytes: number): string {
  const m = bytes / (1024 * 1024)
  return `${Number.isInteger(m) ? m : m.toFixed(2)} MB`
}

/* ── the board as a display CONTRIBUTOR ──────────────────────────────────
 *
 *  A panel soldered to a board is on-board hardware like any other: the board
 *  definition describes it, `esp_board_manager_init()` brings it up, and a
 *  handle for it is sitting there before `app_driver_init()` runs. What the
 *  display framework needs is not another `esp_lcd_*` bring-up — it is the
 *  panel handle bound to LVGL.
 *
 *  So when the board declares a `display_lcd` device, the BOARD contributes
 *  the display framework's `display_node_init` (and the touch registration,
 *  when it declares an `lcd_touch` too). That is the whole adapter: fetch,
 *  bind, log. `drivers/display_co5300_qspi` on the ESP-Mosaico existed only
 *  because framework blocks carry no `cfg:`, and it re-stated fifteen GPIO
 *  numbers the board already knows.
 *
 *  Why the board and not a `drivers/display_board` adapter block: a product
 *  should not have to name a block to use hardware that is already on its
 *  board. The board is where "this device exists" is stated, so it is where
 *  the binding belongs — and a product that changes board changes its panel
 *  with no product.yml edit at all.
 *
 *  This is generated from the board's own YAML (`board_devices.yaml`) and
 *  from nothing else. Pack unreachable → no adapter, and the display
 *  framework's existing "no display panel block in this product" failure is
 *  what the user sees, loudly, rather than a tree that silently has no screen.
 */

/** Panel controllers that only accept a draw window starting on an even
 *  column/row and ending on an odd one. Handed an odd edge, they snap the
 *  window to the 2-pixel grid while the host still streams LVGL's unsnapped
 *  width, so each row lands one pixel further over than the last: text comes
 *  out slanted, on whichever refresh happens to dirty an odd-edged area. An
 *  LVGL rounder keeps every flush on the grid. co5300: the ESP-Mosaico panel,
 *  slant reproduced and cured by the rounder on hardware (2026-10). sh8601:
 *  the waveshare/esp_lcd_sh8601 README asks for the same rounder. */
const EVEN_AREA_PANELS = new Set(['co5300', 'sh8601'])

const BOARD_LCD_ROUNDER = `static void board_lcd_round_to_even(lv_area_t *area)
{
    area->x1 &= ~1;
    area->x2 |= 1;
    area->y1 &= ~1;
    area->y2 |= 1;
}`

/** The LVGL binding for a board-provided panel, as `display_node_init` code.
 *  `s_disp`, `TAG` and the LVGL port are all in scope — this renders inside
 *  app_display.cpp's display_setup_task, after `lvgl_port_init()`.
 *  `evenAreas` wires BOARD_LCD_ROUNDER, which the caller emits as a static. */
function boardDisplayNodeInit(board: string, lcd: BoardDevice, touch: BoardDevice | null, evenAreas: boolean): string {
  const panel = `{
    /* The '${board}' board defines this panel ('${lcd.name}'), so
     * esp_board_manager_init() has already brought it up — bus, panel IO,
     * reset, mirror/swap and disp_on. All that is left is to hand LVGL the
     * handles. No pins appear here on purpose: they are the board's. */
    void *board_lcd = NULL;
    dev_display_lcd_config_t *board_lcd_cfg = NULL;
    if (esp_board_manager_get_device_handle("${lcd.name}", &board_lcd) != ESP_OK ||
        esp_board_manager_get_device_config("${lcd.name}", (void **)&board_lcd_cfg) != ESP_OK ||
        board_lcd == NULL || board_lcd_cfg == NULL) {
        ESP_LOGE(TAG, "board '${board}' declares '${lcd.name}' but it has no handle");
    } else {
        dev_display_lcd_handles_t *board_lcd_h = (dev_display_lcd_handles_t *)board_lcd;
        /* Serial-ish panels (SPI/QSPI, i80, parlio) are pushed pixel by pixel
         * out of an LVGL buffer, so they need a DMA-capable partial buffer and
         * the RGB565 byte swap. RGB and DSI panels own their frame buffer and
         * need neither. */
        const bool board_lcd_serial =
            strcmp(board_lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_SPI) == 0 ||
            strcmp(board_lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_I80) == 0 ||
            strcmp(board_lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_PARLIO) == 0;
        lvgl_port_display_cfg_t board_disp_cfg = {};
        board_disp_cfg.io_handle = board_lcd_h->io_handle;
        board_disp_cfg.panel_handle = board_lcd_h->panel_handle;
        board_disp_cfg.buffer_size = (uint32_t)board_lcd_cfg->lcd_width * 40;
        board_disp_cfg.double_buffer = true;
        board_disp_cfg.hres = board_lcd_cfg->lcd_width;
        board_disp_cfg.vres = board_lcd_cfg->lcd_height;
        board_disp_cfg.color_format =
            (board_lcd_cfg->frame_format == DEV_DISPLAY_LCD_FRAME_FORMAT_RGB888 ||
             board_lcd_cfg->frame_format == DEV_DISPLAY_LCD_FRAME_FORMAT_BGR888)
                ? LV_COLOR_FORMAT_RGB888
                : LV_COLOR_FORMAT_RGB565;
        board_disp_cfg.flags.buff_dma = board_lcd_serial ? 1 : 0;
        board_disp_cfg.flags.swap_bytes = board_lcd_serial ? 1 : 0;${evenAreas ? `
        board_disp_cfg.rounder_cb = board_lcd_round_to_even;` : ''}
        s_disp = lvgl_port_add_disp(&board_disp_cfg);
        ESP_LOGI(TAG, "Panel '${lcd.name}' from board '${board}': %s %dx%d",
                 board_lcd_cfg->chip, board_lcd_cfg->lcd_width, board_lcd_cfg->lcd_height);
    }
}`
  if (!touch) return panel
  return `${panel}

{
    /* Touch from the same board definition ('${touch.name}'). Registered
     * against the panel above, so a panel that failed to bind takes the touch
     * with it rather than registering an input device pointing at nothing. */
    void *board_touch = NULL;
    if (s_disp == NULL) {
        ESP_LOGW(TAG, "no display — skipping board touch '${touch.name}'");
    } else if (esp_board_manager_get_device_handle("${touch.name}", &board_touch) != ESP_OK ||
               board_touch == NULL) {
        ESP_LOGE(TAG, "board '${board}' declares '${touch.name}' but it has no handle");
    } else {
        dev_lcd_touch_handles_t *board_touch_h = (dev_lcd_touch_handles_t *)board_touch;
        lvgl_port_touch_cfg_t board_touch_cfg = {};
        board_touch_cfg.disp = s_disp;
        board_touch_cfg.handle = board_touch_h->touch_handle;
        if (lvgl_port_add_touch(&board_touch_cfg) == NULL) {
            ESP_LOGE(TAG, "board touch '${touch.name}': registration failed");
        } else {
            ESP_LOGI(TAG, "Touch '${touch.name}' from board '${board}'");
        }
    }
}`
}

function boardRendered(board: string, facts: BoardFacts | null): RenderedInstance {
  const lcd = boardDeviceOfType(facts, 'display_lcd')
  const touch = boardDeviceOfType(facts, 'lcd_touch')
  const slots: Record<string, string> = {
    main_includes: '#include "esp_board_manager.h"',
    board_init: `/* On-board hardware from the '${board}' board definition — before the\n   drivers, which may ask the board for a handle. */\nESP_ERROR_CHECK(esp_board_manager_init());`,
  }
  const cmake: Record<string, string[]> = { main: ['espressif__esp_board_manager'] }
  if (lcd) {
    slots.display_includes = [
      '#include <string.h>',
      '#include "esp_board_manager.h"',
      '#include "dev_display_lcd.h"',
      ...(touch ? ['#include "dev_lcd_touch.h"'] : []),
    ].join('\n')
    const evenAreas = EVEN_AREA_PANELS.has((lcd.chip ?? '').toLowerCase())
    if (evenAreas) slots.display_statics = BOARD_LCD_ROUNDER
    slots.display_node_init = boardDisplayNodeInit(board, lcd, touch, evenAreas)
    cmake.app_display = ['espressif__esp_board_manager']
  }
  return {
    block: {
      id: `_board/${board}`,
      kind: 'framework',
      description: `esp-board-manager board '${board}'`,
      idf_components: { 'espressif/esp_board_manager': BOARD_MANAGER_VERSION },
      cmake_priv_requires: cmake,
    },
    prefix: 'BOARD',
    cfg: {},
    slots,
  }
}

/* ── the amend overlay: a product's EXTERNAL wiring, as a board patch ─────
 *
 *  A board definition describes what is soldered to the board. Everything the
 *  user wired themselves is the product's own business — and today nothing
 *  reconciles the two, so a product can put a relay on the pin the board
 *  already drives an LED with and both halves are individually correct.
 *
 *  esp-board-manager's amend mechanism is exactly the missing join: a
 *  manifest plus YAML fragments, applied with `idf.py bmgr -b <board> -a
 *  <dir>`, merged into the board's own peripherals/devices before code
 *  generation. Once a product's external parts are in there, bmgr's IO
 *  conflict check sees product pins and board pins as one set, and
 *  `gen_board_metadata.yaml` — the thing everything downstream should be
 *  asking "which pins are free" — is finally telling the whole truth.
 *
 *  What lands here is DECLARED by blocks (`bmgr:` in block.yml), never
 *  inferred. See BmgrMapping in types.ts for why. */

/** Directory the amend is written into, relative to the generated tree. */
export const AMEND_DIR = 'board_amend'
/** bmgr requires this exact manifest name in the amend directory. */
const AMEND_MANIFEST = 'board_amend.yaml'
/** The one fragment the manifest applies. A single file, not one per block:
 *  `apply:` is an ordered override list, and N per-block files would make the
 *  merge order — hence which of two blocks claiming a pin wins — depend on
 *  instance order in product.yml. One fragment has no such order to get wrong;
 *  a duplicate name is a generation error below instead. */
const AMEND_FRAGMENT = 'zerocode_parts.yaml'

/** Reserved key marking a bmgr fragment as a SHARED resource — see the
 *  shared-key note on BmgrMapping.peripherals. Stripped before emission: bmgr
 *  never sees it, and a board YAML carrying it would be a schema error. */
export const SHARED_KEY = 'shared_key'

/** Every path at which two resolved fragments differ, as `path: a vs b`.
 *  The point of the shared mechanism is that "one bus, two opinions" is named
 *  rather than resolved, so the error has to say WHICH field disagrees —
 *  "these two are different" sends the author back to diff two YAML blobs by
 *  eye, which is how the wrong one gets picked. */
function yamlDiff(a: unknown, b: unknown, at = ''): string[] {
  const isMap = (v: unknown) => v !== null && typeof v === 'object' && !Array.isArray(v)
  if (isMap(a) && isMap(b)) {
    const am = a as Record<string, unknown>
    const bm = b as Record<string, unknown>
    const keys = [...new Set([...Object.keys(am), ...Object.keys(bm)])].sort()
    return keys.flatMap(k => yamlDiff(am[k], bm[k], at ? `${at}.${k}` : k))
  }
  if (JSON.stringify(a) === JSON.stringify(b)) return []
  const show = (v: unknown) => (v === undefined ? '(absent)' : JSON.stringify(v))
  return [`${at || '(value)'}: ${show(a)} vs ${show(b)}`]
}

export interface BoardAmend {
  /** board_amend.yaml — the manifest bmgr reads. */
  manifest: string
  /** The fragment it applies. */
  fragment: string
}

/** Substitute `{{…}}` through an arbitrary YAML value, PRESERVING TYPE.
 *
 *  `substitute` is string-in/string-out, which is right for C slots and wrong
 *  here: bmgr's schema is typed, and `pin: "4"` is not `pin: 4`. So a value
 *  that is nothing but one placeholder resolves to the looked-up value itself
 *  (a number stays a number); anything else is ordinary string interpolation,
 *  which is what `gpio_{{prefix_lc}}_relay` needs. */
function substituteValue(node: unknown, ctx: Record<string, unknown>): unknown {
  if (typeof node === 'string') {
    const whole = /^\{\{\s*([^}]+?)\s*\}\}$/.exec(node)
    if (whole) {
      const value = lookupSoft(whole[1], ctx)
      if (value === undefined) throw new Error(`unresolved placeholder: {{${whole[1]}}}`)
      return value
    }
    return substitute(node, ctx)
  }
  if (Array.isArray(node)) return node.map(n => substituteValue(n, ctx))
  if (node !== null && typeof node === 'object') {
    const out: Record<string, unknown> = {}
    for (const [k, v] of Object.entries(node as Record<string, unknown>)) {
      out[substitute(k, ctx)] = substituteValue(v, ctx)
    }
    return out
  }
  return node
}

/** Build the amend for a `board:` product from its instances' `bmgr:` maps.
 *
 *  Returns null when there is nothing to say — no board, or no instance
 *  declaring a mapping — and then no amend directory is written at all, so
 *  `idf.py bmgr` is invoked with exactly the arguments it was before.
 *
 *  THROWS rather than skipping when a mapping's guards do not hold. See
 *  BmgrGuard: a mapping hands a physical line to code the block author did
 *  not write, so "this instance is outside what I vouched for" has to stop the
 *  build, not quietly produce a tree that looks like the block never had a
 *  mapping. */
/** The first failed `bmgr.requires` guard of `block` for this instance's
 *  `cfg`, as the message the product's author sees — or null when every guard holds.
 *  A malformed guard (no cfg/equals/reason, or a cfg the block does not
 *  declare) is a block-author error and throws, as it always did. */
export function bmgrGuardMiss(block: Block, cfg: Record<string, unknown>, where: string, bmgrBoard: string): string | null {
  for (const guard of block.bmgr?.requires ?? []) {
    if (!guard || typeof guard.cfg !== 'string' || guard.reason === undefined) {
      throw new Error(`block ${block.id}: each bmgr.requires entry needs 'cfg', 'equals' and 'reason'`)
    }
    if (!(guard.cfg in (block.params ?? {}))) {
      // A guard on a param that does not exist always "passes" by accident.
      throw new Error(`block ${block.id}: bmgr.requires guards cfg.${guard.cfg}, which the block does not declare`)
    }
    const actual = cfg[guard.cfg]
    if (String(actual) !== String(guard.equals)) {
      return (
        `${where} was not handed to the board manager on '${bmgrBoard}': cfg.${guard.cfg} is ` +
        `${JSON.stringify(actual)}, and the block's bmgr mapping is only declared valid for ${JSON.stringify(guard.equals)}. ` +
        `It generated with the block's own init instead (the pin is not in the board's occupied-IO list). ` +
        `Reason: ${String(guard.reason).trim()}`
      )
    }
  }
  return null
}

export function buildBoardAmend(
  product: Product,
  rendered: RenderedInstance[],
  bmgrBoard: string | null,
  /** The resolved board directory, when the pack is readable. With it, an
   *  amend peripheral that CONFIGURES a bus the board already provides (same
   *  `type:`, same `config.port`) is refused — esp_board_manager_init() would
   *  bring the port up twice and abort at boot (the second i2c_new_master_bus
   *  on a port fails), which no build catches. Null (no pack on this machine)
   *  skips the guard, the same stance as every other board-pack check; the
   *  validator's mirror of this rule still runs wherever packs exist. */
  boardDir: string | null = null,
): BoardAmend | null {
  if (!bmgrBoard) return null

  const peripherals: unknown[] = []
  const devices: unknown[] = []
  /** bmgr name → the instance that already claimed it. */
  const claimed = new Map<string, string>()
  /** shared_key → the instance that first emitted it, and what it emitted.
   *  This is the whole shared-resource mechanism: a bus is one peripheral no
   *  matter how many sensors hang off it, so the SECOND instance resolving a
   *  key emits nothing — unless it disagrees, which is an error rather than a
   *  merge, because bmgr's field-by-field merge would otherwise decide which
   *  of two SDA pins the bus really runs on by instance order. */
  const shared = new Map<string, { where: string; value: unknown }>()
  const contributors: string[] = []

  for (const r of rendered) {
    const map = r.block.bmgr
    if (!map) continue
    const where = `${r.block.id}${r.prefix ? ` (prefix=${r.prefix})` : ''}`

    if (map.init !== 'board_manager') {
      throw new Error(
        `block ${r.block.id}: bmgr.init must be 'board_manager' (got '${String(map.init)}'). ` +
        `A mapping has to say what takes the hardware over; no other value is implemented yet.`,
      )
    }

    // Guards were judged at render time (bmgrGuardMiss); a failed one means
    // this instance kept its own init and contributes nothing to the amend.
    if (r.bmgrFallback) continue

    const ctx = { cfg: r.cfg, prefix: r.prefix, prefix_lc: r.prefix.toLowerCase() }
    for (const [kind, list, sink] of [
      ['peripheral', map.peripherals ?? [], peripherals],
      ['device', map.devices ?? [], devices],
    ] as Array<['peripheral' | 'device', unknown[], unknown[]]>) {
      for (const entry of list) {
        const resolved = substituteValue(entry, ctx) as Record<string, unknown>
        const name = typeof resolved?.name === 'string' ? resolved.name : ''
        if (!name) {
          throw new Error(`block ${r.block.id}: bmgr ${kind} entry has no 'name' — bmgr merges fragments by name`)
        }

        // PROVIDER-SIDE bus collision with the BOARD. Consumers already adopt
        // a board-owned bus (they resolve the port at runtime); a PROVIDER
        // fragment on that port is a second i2c_new_master_bus() inside
        // esp_board_manager_init(), which fails and aborts the boot. Checked
        // per resolved entry so a shared_key bus is caught on its first (and
        // only) emission.
        if (kind === 'peripheral' && boardDir) {
          const busType = typeof resolved?.type === 'string' ? resolved.type : ''
          const busPort = (resolved?.config as Record<string, unknown> | undefined)?.port
          const boardPeriph = busType && busPort !== undefined
            ? boardProvidesBus(boardDir, busType, String(busPort))
            : null
          if (boardPeriph) {
            throw new Error(
              `product '${product.id}': ${where} configures ${busType} port ${String(busPort)}, ` +
              `but board '${bmgrBoard}' already provides that bus as its peripheral '${boardPeriph}'. ` +
              `esp_board_manager_init() would create the bus twice and abort at boot ` +
              `(the second i2c_new_master_bus on a port fails).\n` +
              `  Fix: drop the ${r.block.id} instance — the board provides port ${String(busPort)}, ` +
              `and consumers on that port adopt the board's bus automatically.`,
            )
          }
        }

        // Shared resource? Emit once per distinct RESOLVED key; refuse two
        // different answers for one key.
        if (SHARED_KEY in resolved) {
          const rawKey = resolved[SHARED_KEY]
          delete resolved[SHARED_KEY]
          if (typeof rawKey !== 'string' && typeof rawKey !== 'number') {
            throw new Error(
              `block ${r.block.id}: bmgr ${kind} '${name}' has a ${SHARED_KEY} that resolved to ` +
              `${JSON.stringify(rawKey)} — it must be a string or number identifying the shared resource, ` +
              `e.g. ${SHARED_KEY}: '${kind === 'peripheral' ? 'i2c' : 'dev'}-{{cfg.port}}'.`,
            )
          }
          const sk = `${kind}:${String(rawKey)}`
          const prior = shared.get(sk)
          if (prior) {
            const diffs = yamlDiff(prior.value, resolved)
            if (diffs.length > 0) {
              throw new Error(
                `product '${product.id}': ${prior.where} and ${where} both describe the shared ` +
                `${kind} '${String(rawKey)}', but they do not agree:\n` +
                diffs.map(d => `    ${d}\n`).join('') +
                `  One physical ${kind} cannot have two configurations, and bmgr would merge them ` +
                `field by field — whichever instance came last would silently win.\n` +
                `  Fix: give the two instances the same configuration, or put them on different ` +
                `buses so their ${SHARED_KEY}s differ.`,
              )
            }
            continue   // same resource, already emitted — exactly once
          }
          shared.set(sk, { where, value: resolved })
        }

        const key = `${kind}:${name}`
        const prior = claimed.get(key)
        if (prior) {
          throw new Error(
            `product '${product.id}': two instances both emit the bmgr ${kind} '${name}' ` +
            `(${prior} and ${where}). bmgr merges same-named entries field by field, so one ` +
            `would silently overwrite the other — template the name with {{prefix_lc}}.`,
          )
        }
        claimed.set(key, where)
        sink.push(resolved)
      }
    }
    contributors.push(where)
  }

  if (peripherals.length === 0 && devices.length === 0) return null

  const header =
    `# GENERATED by the ZeroCode block engine — do not edit.\n` +
    `#\n` +
    `# The parts product '${product.id}' wires EXTERNALLY to board '${bmgrBoard}'.\n` +
    `# Applied over the board definition by \`idf.py bmgr -b ${bmgrBoard} -a ${AMEND_DIR}\`,\n` +
    `# so the board manager owns these lines and its IO-conflict check sees them\n` +
    `# alongside the board's own.\n` +
    `#\n` +
    contributors.map(c => `#   ${c}\n`).join('')

  const body: Record<string, unknown> = {}
  if (peripherals.length > 0) body.peripherals = peripherals
  if (devices.length > 0) body.devices = devices

  const manifest = stringifyYaml({
    version: '1.0',
    description: `ZeroCode: external wiring for product '${product.id}'`,
    apply: [AMEND_FRAGMENT],
  })

  return { manifest: `# GENERATED by the ZeroCode block engine — do not edit.\n${manifest}`, fragment: header + stringifyYaml(body) }
}

/** The companion-radio stack the base manifest pins for the ESP32-P4. */
export const P4_HOSTED_PACKAGES = ['espressif/esp_wifi_remote', 'espressif/esp_hosted']

async function mergeIdfComponents(
  baseYmlPath: string,
  rendered: RenderedInstance[],
  dropFromBase: readonly string[] = [],
): Promise<string> {
  const baseText = await fs.readFile(baseYmlPath, 'utf-8')
  const base = parseYaml(baseText) as IdfComponentYml
  const merged: IdfComponentYml = { dependencies: { ...base.dependencies } }
  for (const pkg of dropFromBase) delete merged.dependencies[pkg]
  for (const r of rendered) {
    for (const [pkg, ver] of Object.entries(r.block.idf_components ?? {})) {
      if (!merged.dependencies[pkg]) merged.dependencies[pkg] = ver
    }
  }
  return stringifyYaml(merged)
}

// ── filesystem helpers ────────────────────────────────────────────────

async function copyTree(src: string, dst: string): Promise<void> {
  const entries = await fs.readdir(src, { withFileTypes: true })
  await fs.mkdir(dst, { recursive: true })
  for (const ent of entries) {
    if (COPY_EXCLUDE.has(ent.name)) continue
    const s = path.join(src, ent.name)
    const d = path.join(dst, ent.name)
    if (ent.isDirectory()) await copyTree(s, d)
    else await fs.copyFile(s, d)
  }
}

async function fileExists(p: string): Promise<boolean> {
  try { await fs.access(p); return true } catch { return false }
}

async function writeFileMk(p: string, content: string): Promise<void> {
  await fs.mkdir(path.dirname(p), { recursive: true })
  await fs.writeFile(p, content)
}
