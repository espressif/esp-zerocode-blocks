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
  type Product,
  type RenderedInstance,
  type ValueType,
  bmgrBoardOf,
  boardTakesOver,
} from './types.js'
import { loadBlock, loadBoardFile, type CatalogPaths } from './loader.js'
import { boardProvidesBus, boardsRoot, findBoardDir } from './validator.js'
import { CHIP_MODULES, normalizeChipTarget } from './hardware.js'

const VALUE_TYPE_TO_UNION_MEMBER: Record<ValueType, string> = {
  bool: 'b',
  u8: 'u8',
  i16: 'i16',
  u16: 'u16',
  u32: 'u32',
}

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
}

export interface GenerateResult {
  productId: string
  outDir: string
  blocks: string[]
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
  const { product, outDir } = input

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
  const boardFacts = bmgrBoard ? await loadBoardFacts(paths, bmgrBoard) : null

  const instanceRendered = await renderInstances(paths, product, bmgrBoard, boardFacts, chip)
  const frameworkRendered = await renderFrameworkBlocks(paths, product, chip)
  const rendered = [...instanceRendered, ...frameworkRendered]
  // A board product carries one extra, synthetic contributor — see
  // boardRendered(). It is deliberately NOT spliced into `rendered`: it must
  // reach main's slots, main's PRIV_REQUIRES, the merged idf_component.yml and
  // — when the board carries a panel — app_display.cpp, and nothing else, so
  // that a product with no board definition and one with it differ only there.
  const boardExtras: RenderedInstance[] = bmgrBoard ? [boardRendered(bmgrBoard, boardFacts)] : []

  await fs.rm(outDir, { recursive: true, force: true })
  await copyTree(paths.baseFirmwareDir, outDir)

  for (const r of frameworkRendered) {
    const blockComponentsDir = path.join(paths.templatesDir, 'code_blocks', r.block.id, 'components')
    if (await fileExists(blockComponentsDir)) {
      await copyTree(blockComponentsDir, path.join(outDir, 'components'))
    }
  }

  await writeFileMk(
    path.join(outDir, 'components/app_driver/include/app_driver_types.h'),
    genAppDriverTypesH(rendered),
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
  const hasMatter = rendered.some(r => isFramework(r, 'matter'))
  if (hasMatter) {
    await writeFileMk(
      path.join(outDir, 'components/app_matter/app_matter.cpp'),
      genAppMatterCpp(rendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_matter/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_matter.cpp"
    INCLUDE_DIRS "include"
    REQUIRES espressif__esp_matter
    PRIV_REQUIRES app_driver app_config
)
`,
      )
    }
  }
  const hasRainmaker = rendered.some(r => isFramework(r, 'rainmaker'))
  if (hasRainmaker) {
    await writeFileMk(
      path.join(outDir, 'components/app_rainmaker/app_rainmaker.cpp'),
      genAppRainmakerCpp(rendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_rainmaker/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_rainmaker.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__esp_rainmaker espressif__rmaker_app_network espressif__network_provisioning esp_event app_driver app_config
)
`,
      )
    }
  }
  const hasZigbee = rendered.some(r => isFramework(r, 'zigbee'))
  if (hasZigbee) {
    await writeFileMk(
      path.join(outDir, 'components/app_zigbee/app_zigbee.cpp'),
      genAppZigbeeCpp(rendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_zigbee/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_zigbee.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__esp-zigbee-lib app_driver app_config app_console console app_update esp_partition esp_timer
)
`,
      )
    }
  }
  const hasBleMesh = rendered.some(r => isFramework(r, 'ble_mesh'))
  if (hasBleMesh) {
    await writeFileMk(
      path.join(outDir, 'components/app_ble_mesh/app_ble_mesh.cpp'),
      genAppBleMeshCpp(rendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_ble_mesh/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_ble_mesh.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES bt nvs_flash app_driver app_config app_console console esp_timer
)
`,
      )
    }
  }
  const hasWebui = rendered.some(r => isFramework(r, 'webui'))
  if (hasWebui) {
    await writeFileMk(
      path.join(outDir, 'components/app_webui/app_webui.cpp'),
      genAppWebuiCpp(rendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_webui/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_webui.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_server esp_wifi esp_netif esp_event nvs_flash espressif__mdns app_driver app_config
)
`,
      )
    }
  }
  const hasEspnow = rendered.some(r => isFramework(r, 'espnow'))
  if (hasEspnow) {
    await writeFileMk(
      path.join(outDir, 'components/app_espnow/app_espnow.cpp'),
      genAppEspnowCpp(rendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_espnow/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_espnow.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_timer console app_driver app_config
)
`,
      )
    }
  }
  const hasBleHid = rendered.some(r => isFramework(r, 'ble_hid'))
  if (hasBleHid) {
    await writeFileMk(
      path.join(outDir, 'components/app_ble_hid/app_ble_hid.cpp'),
      genAppBleHidCpp(rendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_ble_hid/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_ble_hid.cpp" "ble_hid_gap.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_hid bt esp_event nvs_flash app_driver app_config
)
`,
      )
    }
  }
  const hasVision = rendered.some(r => isFramework(r, 'vision'))
  if (hasVision) {
    await writeFileMk(
      path.join(outDir, 'components/app_vision/app_vision.cpp'),
      genAppVisionCpp(rendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_vision/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_vision.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_driver app_config
)
`,
      )
    }
  }
  const hasDisplay = rendered.some(r => isFramework(r, 'display'))
  if (hasDisplay) {
    // The BOARD goes first. It is a panel contributor like any driver block
    // (see boardRendered), and the panel must bind before the touch that
    // registers against it — ordering here is the same rule product.yml
    // already has, applied to the one contributor that is not in product.yml.
    const displayRendered = [...boardExtras, ...rendered]
    if (!displayRendered.some(r => (r.slots['display_node_init'] ?? '').trim())) {
      // Loud at GENERATION, because the alternative is a display product whose
      // only symptom is a dark screen. A boardless product still reaches the
      // framework's own runtime check; a board product does not get to
      // discover on the bench that its pack was not readable.
      throw new Error(
        `product '${product.id}' uses the display framework but nothing provides a panel` +
        (bmgrBoard
          ? `. Board '${bmgrBoard}' declares no display_lcd device` +
            (boardFacts ? '' : ` and its pack could not be read (set ZC_BOARDS_DIR)`) +
            `, and the product instantiates no panel driver block.`
          : ` — add a panel driver block (e.g. drivers/display_ili9341_spi).`),
      )
    }
    await writeFileMk(
      path.join(outDir, 'components/app_display/app_display.cpp'),
      genAppDisplayCpp(displayRendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_display/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_display.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES lvgl__lvgl espressif__esp_lvgl_port esp_lcd app_driver app_config
)
`,
      )
    }
  }
  const hasMqtt = rendered.some(r => isFramework(r, 'mqtt'))
  if (hasMqtt) {
    await writeFileMk(
      path.join(outDir, 'components/app_mqtt/app_mqtt.cpp'),
      genAppMqttCpp(rendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_mqtt/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_mqtt.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__mqtt espressif__cjson esp_wifi esp_netif esp_event nvs_flash app_driver app_config app_console console
)
`,
      )
    }
  }
  const hasSound = rendered.some(r => isFramework(r, 'sound'))
  if (hasSound) {
    await writeFileMk(
      path.join(outDir, 'components/app_sound/app_sound.cpp'),
      genAppSoundCpp(rendered),
    )
    await writeFileMk(
      path.join(outDir, 'components/app_sound/CMakeLists.txt'),
      `idf_component_register(
    SRCS "app_sound.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_driver app_config
)
`,
    )
  }
  const hasAgents = rendered.some(r => isFramework(r, 'agents'))
  if (hasAgents) {
    await writeFileMk(
      path.join(outDir, 'components/app_agents/app_agents.cpp'),
      genAppAgentsCpp(rendered),
    )
    await writeFileMk(
      path.join(outDir, 'components/app_agents/CMakeLists.txt'),
      `idf_component_register(
    SRCS "app_agents.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES agent esp_wifi esp_netif esp_event nvs_flash app_driver app_config app_console console
)
`,
    )
  }
  const hasAudio = rendered.some(r => isFramework(r, 'audio'))
  if (hasAudio) {
    await writeFileMk(
      path.join(outDir, 'components/app_audio/app_audio.cpp'),
      genAppAudioCpp(rendered),
    )
    if (hasDeviceTypes) {
      await writeFileMk(
        path.join(outDir, 'components/app_audio/CMakeLists.txt'),
        `idf_component_register(
    SRCS "app_audio.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__esp-sr app_driver app_config esp_driver_i2s
)
`,
      )
    }
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
  // Add the per-behavior files to app_logic's CMakeLists SRCS. PRIV_REQUIRES is
  // left intact (applyCmakePrivRequires below appends block deps). If the base
  // scaffold didn't ship a CMakeLists (minimal test fixtures), write a fresh one.
  const logicCmakePath = path.join(outDir, 'components/app_logic/CMakeLists.txt')
  const srcs = ['app_logic.cpp', ...appLogic.parts.map(p => p.name)].map(s => `"${s}"`).join(' ')
  let logicCmake: string
  try {
    logicCmake = (await fs.readFile(logicCmakePath, 'utf-8')).replace(/SRCS\s+"app_logic\.cpp"/, `SRCS ${srcs}`)
  } catch {
    logicCmake = `idf_component_register(\n    SRCS ${srcs}\n    INCLUDE_DIRS "include"\n    PRIV_REQUIRES app_driver app_config app_utils nvs_flash esp_timer espressif__button driver esp_driver_gpio esp_driver_ledc esp_driver_i2s esp_driver_spi esp_driver_pcnt esp_driver_rmt esp_driver_uart esp_driver_gptimer\n)\n`
  }
  await writeFileMk(logicCmakePath, logicCmake)

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
    if (needed > boardFacts.flashBytes) {
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
      throw new Error(`partition_table '${product.partition_table}' not found at ${ptSrc}: ${(e as Error).message}`)
    }
    try {
      const ptBlock = await loadBlock(paths, `partition-tables/${product.partition_table}`)
      for (const [k, v] of Object.entries(ptBlock.sdkconfig ?? {})) partitionSdkconfig[k] = String(v)
    } catch { /* a bare partitions.csv with no block.yml is fine */ }
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
  // needs a radio.
  const needsRadio = rendered.some(r => r.block.kind === 'framework' && r.block.radio === true)
  const mergedYml = await mergeIdfComponents(
    path.join(paths.baseFirmwareDir, 'main/idf_component.yml'),
    [...rendered, ...boardExtras],
    chip === 'esp32p4' && !needsRadio ? P4_HOSTED_PACKAGES : [],
  )
  await fs.writeFile(path.join(outDir, 'main/idf_component.yml'), mergedYml)

  return {
    productId: product.id,
    outDir,
    blocks: product.instances.map(i => i.block),
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
    const boardOwns =
      !!bmgrBoard &&
      block.bmgr?.init === 'board_manager' &&
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
    }
    const replaced = new Set(boardOwns ? block.bmgr?.replaces_slots ?? [] : [])
    for (const [slot, body] of Object.entries(block.slots ?? {})) {
      if (replaced.has(slot)) continue
      slots[slot] = substitute(body, ctx)
    }
    out.push({ block, prefix: inst.prefix, cfg, slots, replacedSlots: [...replaced] })
  }
  return out
}

function isFramework(r: RenderedInstance, name: string): boolean {
  return r.block.id === `frameworks/${name}`
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

function collectSlot(rendered: RenderedInstance[], slotName: string, indent = 0): string {
  const pad = ' '.repeat(indent)
  const parts = rendered
    .map(r => r.slots[slotName])
    .filter((s): s is string => Boolean(s && s.trim()))
  if (parts.length === 0) return ''
  return parts
    .map(p => p.replace(/^\n+|\n+$/g, '').split('\n').map(line => (line.length ? pad + line : line)).join('\n'))
    .join('\n\n')
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

function collectParamIds(rendered: RenderedInstance[]): Array<{ name: string; type: ValueType }> {
  const map = new Map<string, ValueType>()
  for (const r of rendered) {
    for (const [cfgKey, valType] of Object.entries(r.block.param_refs ?? {})) {
      const paramName = String(r.cfg[cfgKey] ?? '')
      if (!paramName) {
        // An optional param (schema not `required`) left empty means "feature
        // disabled" — the corresponding {{#if cfg.X}} slot block is dropped, so
        // there's no enum entry to emit. Only a missing REQUIRED param is fatal.
        const schema = r.block.params?.[cfgKey]
        if (schema && schema.required !== true) continue
        throw new Error(`block ${r.block.id} expects cfg.${cfgKey} (param-id reference)`)
      }
      const prev = map.get(paramName)
      if (prev && prev !== valType) {
        throw new Error(`param ${paramName}: conflicting types ${prev} vs ${valType} (block ${r.block.id})`)
      }
      map.set(paramName, valType)
    }
  }
  return [...map.entries()].map(([name, type]) => ({ name, type }))
}

function genAppDriverTypesH(rendered: RenderedInstance[]): string {
  const params = collectParamIds(rendered)
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

#define APP_DRIVER_MAX_SOLUTIONS 8
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

function genAppMatterCpp(rendered: RenderedInstance[]): string {
  // Each endpoint's create block becomes its own static function — isolating
  // its `cfg`/`ep` locals (which would collide across endpoints in one scope)
  // and removing the bare `{ }` block. The body's `return ESP_FAIL` now returns
  // from this function, which the caller checks.
  const usedEp = new Set<string>()
  const epFns: string[] = []
  const epCalls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['matter_endpoint_create'] ?? '').trim()) continue
    const slug = (r.prefix || r.block.id.split('/').pop() || 'ep').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; usedEp.has(name); n++) name = `${slug}_${n}`
    usedEp.add(name)
    epFns.push(`static esp_err_t zc_${name}_matter_endpoint_init(node_t *node)\n{\n${indentText(unwrapInitBody(r.slots['matter_endpoint_create']), 4)}\n    return ESP_OK;\n}`)
    epCalls.push(`    if (zc_${name}_matter_endpoint_init(node) != ESP_OK) return ESP_FAIL;`)
  }
  // Defaulted slot: a `matter_last_fabric_cases` contribution replaces this code.
  // Some handler must exist — with none, a device removed from its last fabric
  // can never be commissioned again.
  const lastFabricCases = collectSlot(rendered, 'matter_last_fabric_cases', 4) ||
`    if (event->Type == chip::DeviceLayer::DeviceEventType::kFabricRemoved &&
        chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
        ESP_LOGW(TAG, "Last fabric removed — factory reset to out-of-box state");
        esp_matter::factory_reset();
    }`
  return `#include "app_matter.h"
#include "app_driver.h"
#include "app_config.h"

#include <esp_log.h>
#include <app/server/Server.h>
#if CONFIG_ENABLE_MATTER_OVER_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif
${collectSlot(rendered, 'matter_includes')}

using namespace esp_matter;
using namespace chip::app::Clusters;

static const char *TAG = "app_matter";

static app_driver_handle_t s_handle = 0;
static bool s_from_driver = false;

${collectSlot(rendered, 'matter_statics')}

static void matter_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
${collectSlot(rendered, 'matter_driver_cb_cases', 4)}
}

static esp_err_t app_attribute_update_cb(
    attribute::callback_type_t type,
    uint16_t endpoint_id,
    uint32_t cluster_id,
    uint32_t attribute_id,
    esp_matter_attr_val_t *val,
    void *priv_data)
{
    if (type == attribute::PRE_UPDATE) {
        if (s_from_driver) return ESP_OK;
${collectSlot(rendered, 'matter_attr_cb_cases', 8)}
    }
    return ESP_OK;
}

static esp_err_t app_identification_cb(
    identification::callback_type_t type,
    uint16_t endpoint_id,
    uint8_t effect_id,
    uint8_t effect_variant,
    void *priv_data)
{
    ESP_LOGI(TAG, "Identification: type=%d, endpoint=%d, effect=%d", type, endpoint_id, effect_id);
${collectSlot(rendered, 'matter_identify_cases', 4)}
    return ESP_OK;
}

static void app_matter_event_cb(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t arg)
{
${lastFabricCases}
${collectSlot(rendered, 'matter_event_cases', 4)}
}

${epFns.join('\n\n')}

#if CONFIG_ENABLE_MATTER_OVER_THREAD
/* The Thread platform config, as a weak default rather than a fixed one: this
 * covers a chip driving its own 802.15.4 radio, which is every Thread-capable
 * target here (esp32c5 / esp32c6 / esp32h2). A product that needs something
 * else — an external radio co-processor over UART/SPI, a different storage
 * partition, deeper queues — defines a strong
 * zc_openthread_platform_config() in any of its components and that one
 * wins at link time, with no change to this generator or to the
 * matter_thread fragment. The RCP transports need port/baud/pin values that
 * only the board knows, which is why they are not derived from Kconfig here. */
extern "C" __attribute__((weak))
esp_openthread_platform_config_t zc_openthread_platform_config(void)
{
    esp_openthread_platform_config_t cfg = {
        .radio_config = { .radio_mode = RADIO_MODE_NATIVE },
        .host_config  = { .host_connection_mode = HOST_CONNECTION_MODE_NONE },
        .port_config  = {
            .storage_partition_name = "nvs",
            .netif_queue_size       = 10,
            .task_queue_size        = 10,
        },
    };
    return cfg;
}
#endif

esp_err_t app_matter_init(void)
{
    s_handle = app_driver_register_solution("matter", matter_driver_cb, NULL);

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return ESP_FAIL;
    }

${epCalls.join('\n')}

#if CONFIG_ENABLE_MATTER_OVER_THREAD
    /* Matter's ThreadStackManagerImpl::_InitThreadStack() passes the launcher's
     * file-static config straight to esp_openthread_init(). Nothing populates
     * that pointer unless it is set here, and the assert() guarding it is
     * compiled out at this optimization level — so leaving it unset does not
     * fail loudly, it faults on the first dereference inside the OpenThread
     * stack. */
    static esp_openthread_platform_config_t ot_platform_config =
        zc_openthread_platform_config();
    ESP_ERROR_CHECK(set_openthread_platform_config(&ot_platform_config));
#endif

    ESP_ERROR_CHECK(esp_matter::start(app_matter_event_cb));
    ESP_LOGI(TAG, "Matter initialized");
    return ESP_OK;
}
`
}

function genAppRainmakerCpp(rendered: RenderedInstance[]): string {
  // Mirror of genAppMatterCpp: each device-type instance's
  // rainmaker_device_create slot becomes its own static init function
  // (isolating locals), called in sequence from app_rainmaker_init().
  const usedDev = new Set<string>()
  const devFns: string[] = []
  const devCalls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['rainmaker_device_create'] ?? '').trim()) continue
    const slug = (r.prefix || r.block.id.split('/').pop() || 'dev').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; usedDev.has(name); n++) name = `${slug}_${n}`
    usedDev.add(name)
    devFns.push(`static esp_err_t zc_${name}_rainmaker_device_init(esp_rmaker_node_t *node)\n{\n${indentText(unwrapInitBody(r.slots['rainmaker_device_create']), 4)}\n    return ESP_OK;\n}`)
    devCalls.push(`    if (zc_${name}_rainmaker_device_init(node) != ESP_OK) return ESP_FAIL;`)
  }
  return `#include "app_rainmaker.h"
#include "app_driver.h"
#include "app_config.h"

#include <esp_log.h>
#include <esp_event.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_common_events.h>
#include <app_network.h>
#include <network_provisioning/manager.h>
${collectSlot(rendered, 'rainmaker_includes')}

static const char *TAG = "app_rainmaker";

static app_driver_handle_t s_handle = 0;

${collectSlot(rendered, 'rainmaker_statics')}

/* Cloud/app write → hardware. Each device-type binding claims its own
 * (device, param) pair and returns; unclaimed writes fall through to OK. */
static esp_err_t rainmaker_write_cb(
    const esp_rmaker_device_t *device,
    const esp_rmaker_param_t *param,
    const esp_rmaker_param_val_t val,
    void *priv_data,
    esp_rmaker_write_ctx_t *ctx)
{
    (void)priv_data;
    if (ctx) {
        ESP_LOGI(TAG, "Write via %s: %s", esp_rmaker_device_cb_src_to_str(ctx->src),
                 esp_rmaker_param_get_name(param));
    }
${collectSlot(rendered, 'rainmaker_write_cb_cases', 4)}
    return ESP_OK;
}

/* Hardware/param bus → cloud. The registry doesn't notify the source
 * solution, so writes we made ourselves in rainmaker_write_cb don't echo. */
static void rainmaker_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
${collectSlot(rendered, 'rainmaker_driver_cb_cases', 4)}
}

/* Provisioning / RainMaker / network lifecycle events (default event loop).
 * Behavior blocks contribute rainmaker_event_cases keyed on
 * (event_base, event_id) — e.g. NETWORK_PROV_EVENT / RMAKER_COMMON_EVENT. */
static void rainmaker_app_event_cb(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
${collectSlot(rendered, 'rainmaker_event_cases', 4)}
}

${devFns.join('\n\n')}

esp_err_t app_rainmaker_init(void)
{
    s_handle = app_driver_register_solution("rainmaker", rainmaker_driver_cb, NULL);

    /* Network stack first: RainMaker core needs Wi-Fi initialized before
     * esp_rmaker_node_init (self-claiming reads the MAC). */
    app_network_init();

    /* RainMaker's own SNTP client: wall-clock time for anything the product
     * keys on it (schedules, daily budgets, night windows, timestamps).
     * Without it time() sits at the 1970 epoch and every such feature
     * silently misbehaves. */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = true,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(
        &rainmaker_cfg, "ZeroCode Device", "Device");
    if (!node) {
        ESP_LOGE(TAG, "Failed to init RainMaker node");
        return ESP_FAIL;
    }

${devCalls.join('\n')}

    /* Node-level features contributed by behavior blocks (OTA, services, …) */
${collectSlot(rendered, 'rainmaker_node_init', 4)}

    esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, rainmaker_app_event_cb, NULL);
    esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID, rainmaker_app_event_cb, NULL);
    esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, rainmaker_app_event_cb, NULL);

    ESP_ERROR_CHECK(esp_rmaker_start());

    /* Starts BLE/SoftAP provisioning via the ESP RainMaker phone app when
     * unprovisioned; connects directly on subsequent boots. */
    esp_err_t err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start network: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "RainMaker initialized");
    return ESP_OK;
}
`
}

function genAppZigbeeCpp(rendered: RenderedInstance[]): string {
  // Mirror of genAppMatterCpp/genAppRainmakerCpp: each device-type instance's
  // zigbee_endpoint_create slot becomes its own static init function that adds
  // its endpoint to the shared device descriptor, called in sequence from the
  // Zigbee task.
  const usedEp = new Set<string>()
  const epFns: string[] = []
  const epCalls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['zigbee_endpoint_create'] ?? '').trim()) continue
    const slug = (r.prefix || r.block.id.split('/').pop() || 'ep').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; usedEp.has(name); n++) name = `${slug}_${n}`
    usedEp.add(name)
    epFns.push(`static esp_err_t zc_${name}_zigbee_endpoint_init(ezb_af_device_desc_t dev_desc)\n{\n${indentText(unwrapInitBody(r.slots['zigbee_endpoint_create']), 4)}\n    return ESP_OK;\n}`)
    epCalls.push(`    if (zc_${name}_zigbee_endpoint_init(dev_desc) != ESP_OK) {\n        ESP_LOGE(TAG, "endpoint init failed");\n        vTaskDelete(NULL);\n        return;\n    }`)
  }
  return `#include "app_zigbee.h"
#include "app_driver.h"
#include "app_config.h"
#include "app_console.h"

#include <esp_log.h>
#include <esp_console.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <stdlib.h>
#include <esp_zigbee.h>
#include <ezbee/zha.h>
${collectSlot(rendered, 'zigbee_includes')}

static const char *TAG = "app_zigbee";

static app_driver_handle_t s_handle = 0;
static volatile bool s_zb_started = false;

${collectSlot(rendered, 'zigbee_statics')}

/* NOT ESP_ERROR_CHECK: starting one BDB mode while another is in flight
 * (steering retry during Finding & Binding, or vice versa) returns an
 * error — that is a busy condition to retry, not a reason to abort.
 * (Found on hardware: zb-bind during steering panicked the first build.) */
static esp_err_t bdb_start_top_level_commissioning(uint8_t mode_mask)
{
    esp_err_t err = esp_zigbee_err_to_esp(ezb_bdb_start_top_level_commissioning(mode_mask));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BDB mode 0x%x not started (%s) — busy, will retry", mode_mask, esp_err_to_name(err));
    }
    return err;
}

/* A one-shot esp_timer (not a task per retry): the callback runs on the shared
 * esp_timer task — off the Zigbee task — so it takes the stack lock, mirroring
 * the SDK's own alarm_timer helper. */
struct bdb_retry_ctx {
    esp_timer_handle_t timer;
    uint8_t mode_mask;
};

static void bdb_commissioning_retry_cb(void *arg)
{
    struct bdb_retry_ctx *ctx = (struct bdb_retry_ctx *)arg;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)bdb_start_top_level_commissioning(ctx->mode_mask);
        esp_zigbee_lock_release();
    }
    esp_timer_delete(ctx->timer);
    free(ctx);
}

static void schedule_bdb_commissioning_retry(uint8_t mode_mask)
{
    struct bdb_retry_ctx *ctx = (struct bdb_retry_ctx *)calloc(1, sizeof(*ctx));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to schedule BDB mode 0x%x retry", mode_mask);
        return;
    }
    ctx->mode_mask = mode_mask;
    const esp_timer_create_args_t args = {
        .callback = bdb_commissioning_retry_cb,
        .arg = ctx,
        .name = "zb_bdb_retry",
    };
    if (esp_timer_create(&args, &ctx->timer) != ESP_OK ||
        esp_timer_start_once(ctx->timer, 1000 * 1000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to schedule BDB mode 0x%x retry", mode_mask);
        if (ctx->timer) {
            esp_timer_delete(ctx->timer);
        }
        free(ctx);
    }
}

/* Hub/controller writes → hardware. Runs ON the Zigbee task (no lock needed). */
static void zb_attribute_handler(ezb_zcl_set_attr_value_message_t *message)
{
    if (!message) return;
    if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "attribute write failed, status %d", message->info.status);
        return;
    }
    uint8_t endpoint = message->info.dst_ep;
    uint16_t cluster_id = message->info.cluster_id;
    uint16_t attr_id = message->in.attribute.id;
    (void)endpoint; (void)cluster_id; (void)attr_id;
${collectSlot(rendered, 'zigbee_attr_cb_cases', 4)}
}

static void zb_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message)
{
    if (callback_id == EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID) {
        zb_attribute_handler((ezb_zcl_set_attr_value_message_t *)message);
        return;
    }
    /* Command-style callbacks (door lock, window covering movement, …) —
     * device-type blocks hook these via zigbee_action_cases, keyed on
     * callback_id + their own message cast. */
${collectSlot(rendered, 'zigbee_action_cases', 4)}
    ESP_LOGI(TAG, "Zigbee action 0x%x", callback_id);
}

/* Hardware/param bus → ZCL attributes. Runs OFF the Zigbee task, so every
 * Zigbee call MUST be wrapped in esp_zigbee_lock_acquire/release (compiles fine
 * without it, corrupts state at runtime). */
static void zigbee_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
    if (!s_zb_started) return;
${collectSlot(rendered, 'zigbee_driver_cb_cases', 4)}
}

/* Push every bus param to its ZCL attribute once the device is on the network.
 * Anything written to the bus while the stack was still starting — the NVS
 * restore of a remembered brightness, a controller's boot-time decision — was
 * dropped by zigbee_driver_cb's started-gate, and the hub kept showing the
 * endpoint's creation-time defaults until the next change. Runs from the
 * esp_timer task, where the driver-cb cases' lock acquire is the normal path. */
static void zigbee_resync_cb(void *arg)
{
    (void)arg;
    for (int id = 0; id < (int)APP_DRIVER_PARAM_MAX; id++) {
        app_driver_param_val_t val = {};
        if (app_driver_get_param((app_driver_param_id_t)id, &val) != ESP_OK) continue;
        zigbee_driver_cb((app_driver_param_id_t)id, val, APP_DRIVER_SOURCE_LOCAL, NULL);
    }
    ESP_LOGI(TAG, "Bus state pushed to the hub");
}

static void zigbee_schedule_resync(void)
{
    static esp_timer_handle_t s_resync = NULL;
    if (!s_resync) {
        const esp_timer_create_args_t args = {
            .callback = &zigbee_resync_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "zb_resync",
            .skip_unhandled_events = true,
        };
        if (esp_timer_create(&args, &s_resync) != ESP_OK) return;
    }
    (void)esp_timer_stop(s_resync);
    (void)esp_timer_start_once(s_resync, 200 * 1000);
}

/* REQUIRED extern: BDB/ZDO lifecycle. Behavior blocks contribute
 * zigbee_signal_cases (sig_type + err_status are in scope). */
static bool zigbee_app_signal_handler(const ezb_app_signal_t *app_signal)
{
    ezb_app_signal_type_t sig_type = ezb_app_signal_get_type(app_signal);
    const void *signal_params = ezb_app_signal_get_params(app_signal);
    ezb_bdb_comm_status_t err_status = EZB_BDB_STATUS_SUCCESS;
    if ((sig_type >> 8) == EZB_APP_SIGNAL_GROUP_BDB && signal_params) {
        err_status = ((const ezb_bdb_signal_simple_params_t *)signal_params)->status;
    }
    switch (sig_type) {
        case EZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Zigbee stack initialized");
            (void)bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
            break;
        case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case EZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == EZB_BDB_STATUS_SUCCESS) {
                if (ezb_bdb_is_factory_new()) {
                    ESP_LOGI(TAG, "Start network steering — open pairing on your Zigbee hub");
                    (void)bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
                } else {
                    ESP_LOGI(TAG, "Device rebooted on network");
                    zigbee_schedule_resync();
                }
            } else {
                ESP_LOGW(TAG, "Stack start failed (status 0x%x), retrying", err_status);
                schedule_bdb_commissioning_retry(EZB_BDB_MODE_INITIALIZATION);
            }
            break;
        case EZB_BDB_SIGNAL_STEERING:
            if (err_status == EZB_BDB_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Joined network successfully");
                zigbee_schedule_resync();
            } else {
                ESP_LOGI(TAG, "Network steering not successful (status 0x%x), retrying in 1s",
                         err_status);
                schedule_bdb_commissioning_retry(EZB_BDB_MODE_NETWORK_STEERING);
            }
            break;
        case EZB_ZDO_SIGNAL_LEAVE:
            ESP_LOGW(TAG, "Left the Zigbee network");
            break;
        default:
            ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: 0x%x",
                     ezb_app_signal_to_string(sig_type), sig_type, err_status);
            break;
    }
${collectSlot(rendered, 'zigbee_signal_cases', 4)}
    return true;
}

${epFns.join('\n\n')}

/* The Zigbee stack owns its own task — app_main must NOT block on joining. */
static void zigbee_task(void *pvParameters)
{
    (void)pvParameters;
    esp_zigbee_config_t zb_cfg = {
        .device_config = {
            .device_type = EZB_NWK_DEVICE_TYPE_ROUTER,
            .install_code_policy = false,
            .zczr_config = {
                .max_children = 10,
            },
        },
        .platform_config = {
            .storage_partition_name = "nvs",
            .radio_config = {
                .radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE,
            },
        },
    };
    ESP_ERROR_CHECK(esp_zigbee_init(&zb_cfg));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_app_signal_add_handler(zigbee_app_signal_handler)));

    ezb_af_device_desc_t dev_desc = ezb_af_create_device_desc();
    if (dev_desc == EZB_INVALID_AF_DEVICE_DESC) {
        ESP_LOGE(TAG, "Failed to create Zigbee device descriptor");
        ESP_ERROR_CHECK(esp_zigbee_deinit());
        vTaskDelete(NULL);
        return;
    }
${epCalls.join('\n')}

    /* Node-level features contributed by behavior blocks (OTA, reporting, …) */
${collectSlot(rendered, 'zigbee_node_init', 4)}

    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_device_desc_register(dev_desc)));
    ezb_zcl_core_action_handler_register(zb_action_handler);
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_bdb_set_primary_channel_set(0x07FFF800U)));
    ESP_ERROR_CHECK(esp_zigbee_start(false));
    s_zb_started = true;
    ESP_LOGI(TAG, "Zigbee initialized");
    (void)esp_zigbee_launch_mainloop();
    s_zb_started = false;
    ezb_app_signal_remove_handler(zigbee_app_signal_handler);
    ESP_ERROR_CHECK(esp_zigbee_deinit());
    vTaskDelete(NULL);
}

/* Finding & Binding initiator: creates ZDO bindings from this node's
 * client clusters to matching targets whose F&B window is open (hubs open
 * it from their UI; devices via their own zb-bind). Controller device types
 * (remotes, scene buttons) need this once to know where their commands go. */
esp_err_t app_zigbee_start_finding_binding(void)
{
    if (!s_zb_started) {
        ESP_LOGW(TAG, "Zigbee stack not started yet");
        return ESP_ERR_INVALID_STATE;
    }
    if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = esp_zigbee_err_to_esp(
        ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_FINDING_N_BINDING));
    esp_zigbee_lock_release();
    ESP_LOGI(TAG, "Finding & Binding started (%s)", esp_err_to_name(err));
    return err;
}

static int zb_bind_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return app_zigbee_start_finding_binding() == ESP_OK ? 0 : 1;
}

esp_err_t app_zigbee_init(void)
{
    s_handle = app_driver_register_solution("zigbee", zigbee_driver_cb, NULL);

    const esp_console_cmd_t bind_cmd = {
        .command = "zb-bind",
        .help = "Start Zigbee Finding & Binding (bind this device's controls to targets)",
        .hint = NULL,
        .func = &zb_bind_cmd,
        .argtable = NULL,
    };
    app_console_register_cmd(&bind_cmd);

    xTaskCreate(zigbee_task, "zigbee_main", 8192, NULL, 5, NULL);
    return ESP_OK;
}
`
}

/**
 * app_ble_mesh.cpp — the BLE Mesh framework.
 *
 * Mirror of genAppZigbeeCpp: the framework owns everything that is the same
 * for every mesh product, device-type and behavior blocks contribute only
 * their own bindings through the ble_mesh_* slot family.
 *
 * The generated node is deliberately opinionated, because each of these was a
 * way a hand-written mesh node silently did not work:
 *
 *  - the BLE host is brought up and SYNCHRONISED before the mesh stack, and
 *    the device UUID's tail is the synchronised address, so two devices never
 *    beacon the same UUID;
 *  - every model — servers and clients alike — is defined WITH a publication
 *    context, so no publish can run on a NULL pub;
 *  - a client message goes out through esp_ble_mesh_generic_client_set_state
 *    with a full message context and an incrementing TID, never as a raw
 *    one-byte publish (a Set without a fresh TID is dropped by the receiver as
 *    a retransmission of the last one);
 *  - the app key index is READ BACK from the client model's own key list at
 *    send time rather than remembered from a bind event, so it survives a
 *    reboot, and an unbound client refuses loudly instead of sending into
 *    nowhere;
 *  - "am I provisioned" is always esp_ble_mesh_node_is_provisioned(), the
 *    stack's own answer restored from settings, never a local flag;
 *  - a factory reset re-enables provisioning from the reset event, so the node
 *    is joinable again without a reboot.
 */
function genAppBleMeshCpp(rendered: RenderedInstance[]): string {
  return `#include "app_ble_mesh.h"
#include "app_driver.h"
#include "app_config.h"
#include "app_console.h"

#include <esp_log.h>
#include <esp_console.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_provisioning_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>
#include <esp_ble_mesh_config_model_api.h>
#include <esp_ble_mesh_generic_model_api.h>

#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
${collectSlot(rendered, 'ble_mesh_includes')}

static const char *TAG = "app_ble_mesh";

#define ZC_MESH_COUNT(a)   (sizeof(a) / sizeof((a)[0]))
#define ZC_MESH_SEND_TTL   7
#define ZC_MESH_BEARERS    ((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT))

/* NimBLE's persistent-store hook. Declared here rather than included because
 * the header lives in a private store directory; it is plain C. */
extern "C" void ble_store_config_init(void);

static app_driver_handle_t s_handle = 0;
static volatile bool s_mesh_ready = false;
/* NetKey the node sends on. Restored from the provisioning event; the primary
 * subnet is the right answer both before that and after a reboot. */
static uint16_t s_net_idx = ESP_BLE_MESH_KEY_PRIMARY;
static uint8_t s_tid = 0;
/* Bytes 0-1 are the ZeroCode marker a provisioner matches on; bytes 2-7 are
 * filled with this device's BLE address once the host has synchronised. */
static uint8_t s_dev_uuid[16] = { ZC_MESH_UUID_B0, ZC_MESH_UUID_B1 };
/* Cleared by a behavior (the provisioner) that must NOT beacon for a
 * provisioner of its own. Everything else advertises when unprovisioned. */
static bool s_advertise_unprovisioned = true;

/* ── BLE host ──────────────────────────────────────────────────────────
 * Mesh drives the controller itself, but the host must be up and its
 * identity address settled first — the address is what makes each node's
 * device UUID unique, so init BLOCKS until sync rather than racing it. */
static SemaphoreHandle_t s_host_sync = NULL;
static uint8_t s_bd_addr[6] = { 0 };

static void ble_host_on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset, reason %d", reason);
}

static void ble_host_on_sync(void)
{
    uint8_t own_addr_type = 0;
    if (ble_hs_util_ensure_addr(0) != 0 || ble_hs_id_infer_auto(0, &own_addr_type) != 0) {
        ESP_LOGE(TAG, "BLE host has no usable identity address");
    } else {
        (void)ble_hs_id_copy_addr(own_addr_type, s_bd_addr, NULL);
    }
    if (s_host_sync) {
        xSemaphoreGive(s_host_sync);
    }
}

static void ble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static esp_err_t ble_host_init(void)
{
    s_host_sync = xSemaphoreCreateBinary();
    if (!s_host_sync) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init the BLE host (%s)", esp_err_to_name(err));
        return err;
    }
    ble_hs_cfg.reset_cb = ble_host_on_reset;
    ble_hs_cfg.sync_cb = ble_host_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();
    nimble_port_freertos_init(ble_host_task);
    xSemaphoreTake(s_host_sync, portMAX_DELAY);
    return ESP_OK;
}

/* ── Composition ───────────────────────────────────────────────────────
 * One element carrying, always: the Config Server (how a provisioner
 * configures us), the Config Client (what a provisioner behavior drives),
 * Generic OnOff/Level SERVERS (what a controller writes to us) and Generic
 * OnOff/Level CLIENTS (what we publish with). A product that uses only half
 * of them pays a few hundred bytes; a product that is missing the half it
 * needs does not work at all, and nothing in the build says so. */
/* The mesh model macros initialise the fields THEY care about and leave the
 * rest to zero-initialisation, which is exactly right and which C++ warns
 * about anyway. Scoped to these definitions only, so a real missing
 * initialiser anywhere else in this file still shows up. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static esp_ble_mesh_cfg_srv_t s_config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_ENABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = ZC_MESH_SEND_TTL,
};

static esp_ble_mesh_client_t s_cfg_client;
static esp_ble_mesh_client_t s_onoff_client;
static esp_ble_mesh_client_t s_level_client;

ESP_BLE_MESH_MODEL_PUB_DEFINE(zc_onoff_srv_pub, 2 + 3, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(zc_level_srv_pub, 2 + 5, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(zc_onoff_cli_pub, 2 + 4, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(zc_level_cli_pub, 2 + 7, ROLE_NODE);

static esp_ble_mesh_gen_onoff_srv_t s_onoff_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
};

static esp_ble_mesh_gen_level_srv_t s_level_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
};

static esp_ble_mesh_model_t s_root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&s_config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&s_cfg_client),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&zc_onoff_srv_pub, &s_onoff_server),
    ESP_BLE_MESH_MODEL_GEN_LEVEL_SRV(&zc_level_srv_pub, &s_level_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&zc_onoff_cli_pub, &s_onoff_client),
    ESP_BLE_MESH_MODEL_GEN_LEVEL_CLI(&zc_level_cli_pub, &s_level_client),
};

/* Written out rather than built with ESP_BLE_MESH_ELEMENT: that macro's empty
 * vendor-model list is a C compound literal, which this translation unit
 * (C++, because it calls the driver bus) cannot use. */
static esp_ble_mesh_elem_t s_elements[] = {
    {
        .element_addr = ESP_BLE_MESH_ADDR_UNASSIGNED,
        .location = 0,
        .sig_model_count = (uint8_t)ZC_MESH_COUNT(s_root_models),
        .vnd_model_count = 0,
        .sig_models = s_root_models,
        .vnd_models = NULL,
    },
};

static esp_ble_mesh_comp_t s_composition = {
    .cid = ZC_MESH_CID,
    .pid = 0,
    .vid = 0,
    .element_count = (uint8_t)ZC_MESH_COUNT(s_elements),
    .elements = s_elements,
};

static esp_ble_mesh_prov_t s_provision = {
    .uuid = s_dev_uuid,
    /* No OOB: the product is provisioned from a phone app or from a kit
     * member running behaviors/ble_mesh_provisioner. */
    .output_size = 0,
    .output_actions = 0,
    .input_size = 0,
    .input_actions = 0,
#if CONFIG_BLE_MESH_PROVISIONER
    /* Only used when a behavior turns this node into the provisioner. */
    .prov_uuid = s_dev_uuid,
    .prov_unicast_addr = 0x0001,
    .prov_start_address = 0x0005,
    .prov_attention = 0x00,
    .prov_algorithm = 0x00,
    .prov_pub_key_oob = 0x00,
    .prov_static_oob_val = NULL,
    .prov_static_oob_len = 0x00,
    .flags = 0x00,
    .iv_index = 0x00,
#endif
};

#pragma GCC diagnostic pop

${collectSlot(rendered, 'ble_mesh_statics')}

/* ── Level scaling ─────────────────────────────────────────────────────
 * ONE definition, used in both directions, so a driver level that goes out
 * onto the mesh and comes back lands on the value it started from. */
int16_t zc_ble_mesh_level_from_u8(uint8_t value)
{
    return (int16_t)((int32_t)value * 65535 / 254 - 32768);
}

uint8_t zc_ble_mesh_level_to_u8(int16_t level)
{
    return (uint8_t)(((int32_t)level + 32768) * 254 / 65535);
}

bool zc_ble_mesh_provisioned(void)
{
    return esp_ble_mesh_node_is_provisioned();
}

/* The app key a client model may send with. Read from the model's own key
 * list every time, because that list is what the stack restores from settings
 * — a value cached at bind time is gone after the first reboot, and sending
 * with ESP_BLE_MESH_KEY_UNUSED fails silently. */
static uint16_t client_app_idx(esp_ble_mesh_client_t *client)
{
    if (!client || !client->model) {
        return ESP_BLE_MESH_KEY_UNUSED;
    }
    for (size_t i = 0; i < ZC_MESH_COUNT(client->model->keys); i++) {
        if (client->model->keys[i] != ESP_BLE_MESH_KEY_UNUSED) {
            return client->model->keys[i];
        }
    }
    return ESP_BLE_MESH_KEY_UNUSED;
}

static esp_err_t client_msg_common(esp_ble_mesh_client_common_param_t *common,
                                   esp_ble_mesh_client_t *client,
                                   uint32_t opcode, uint16_t addr)
{
    uint16_t app_idx = client_app_idx(client);
    if (app_idx == ESP_BLE_MESH_KEY_UNUSED) {
        ESP_LOGW(TAG, "no application key bound to this client model yet — "
                      "provision the node and let the provisioner bind it before sending");
        return ESP_ERR_INVALID_STATE;
    }
    common->opcode = opcode;
    common->model = client->model;
    common->ctx.net_idx = s_net_idx;
    common->ctx.app_idx = app_idx;
    common->ctx.addr = addr;
    common->ctx.send_ttl = ZC_MESH_SEND_TTL;
    common->msg_timeout = 0;    /* 0 = the configured default */
    return ESP_OK;
}

esp_err_t zc_ble_mesh_publish_onoff(uint16_t addr, bool on)
{
    if (!s_mesh_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_ble_mesh_client_common_param_t common = {};
    esp_err_t err = client_msg_common(&common, &s_onoff_client,
                                      ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK, addr);
    if (err != ESP_OK) {
        return err;
    }
    esp_ble_mesh_generic_client_set_state_t set = {};
    set.onoff_set.op_en = false;
    set.onoff_set.onoff = on ? 1 : 0;
    /* A fresh transaction id per message: a receiver discards a Set whose TID
     * repeats the last one from the same source as a retransmission, so a
     * fixed TID makes every command after the first one disappear. */
    set.onoff_set.tid = s_tid++;
    err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "OnOff Set to 0x%04x failed (%s)", addr, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "published OnOff %s to 0x%04x", on ? "ON" : "OFF", addr);
    }
    return err;
}

esp_err_t zc_ble_mesh_publish_level(uint16_t addr, int16_t level)
{
    if (!s_mesh_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_ble_mesh_client_common_param_t common = {};
    esp_err_t err = client_msg_common(&common, &s_level_client,
                                      ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET_UNACK, addr);
    if (err != ESP_OK) {
        return err;
    }
    esp_ble_mesh_generic_client_set_state_t set = {};
    set.level_set.op_en = false;
    set.level_set.level = level;
    set.level_set.tid = s_tid++;
    err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Level Set to 0x%04x failed (%s)", addr, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "published Level %d to 0x%04x", (int)level, addr);
    }
    return err;
}

/* A server model may publish its status only once somebody has configured a
 * publish address for it. Asking the model itself is the honest test — a
 * \"am I provisioned\" check gets this wrong in both directions (a provisioner
 * is not itself a provisioned node, and a provisioned node's server may still
 * have no publication set). */
static bool server_can_publish(esp_ble_mesh_model_t *model)
{
    return model && model->pub && model->pub->publish_addr != ESP_BLE_MESH_ADDR_UNASSIGNED;
}

esp_err_t zc_ble_mesh_set_onoff_state(bool on)
{
    s_onoff_server.state.onoff = on ? 1 : 0;
    s_onoff_server.state.target_onoff = on ? 1 : 0;
    if (!s_mesh_ready || !server_can_publish(s_onoff_server.model)) {
        return ESP_OK;   /* state kept; nobody has asked to be told yet */
    }
    return esp_ble_mesh_model_publish(s_onoff_server.model,
                                      ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                      sizeof(s_onoff_server.state.onoff),
                                      &s_onoff_server.state.onoff, ROLE_NODE);
}

esp_err_t zc_ble_mesh_set_level_state(int16_t level)
{
    s_level_server.state.level = level;
    s_level_server.state.target_level = level;
    if (!s_mesh_ready || !server_can_publish(s_level_server.model)) {
        return ESP_OK;
    }
    return esp_ble_mesh_model_publish(s_level_server.model,
                                      ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_STATUS,
                                      sizeof(s_level_server.state.level),
                                      (uint8_t *)&s_level_server.state.level, ROLE_NODE);
}

esp_err_t zc_ble_mesh_join_mode(void)
{
    if (zc_ble_mesh_provisioned()) {
        ESP_LOGW(TAG, "already provisioned (address 0x%04x) — run \\"mesh reset\\" first",
                 esp_ble_mesh_get_primary_element_address());
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_ble_mesh_node_prov_enable(ZC_MESH_BEARERS);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "advertising as unprovisioned — open your mesh app to add this device");
    } else {
        ESP_LOGW(TAG, "could not start advertising (%s)", esp_err_to_name(err));
    }
    return err;
}

esp_err_t zc_ble_mesh_factory_reset(void)
{
    /* Resets the node AND erases its stored mesh settings. The reset event
     * (handled below) puts it straight back into unprovisioned advertising,
     * so this is recoverable without a reboot. */
    esp_err_t err = esp_ble_mesh_node_local_reset();
    ESP_LOGW(TAG, "left the mesh network (%s)", esp_err_to_name(err));
    return err;
}

/* ── Mesh → hardware ───────────────────────────────────────────────────
 * Device types translate an incoming Generic state into their own driver
 * params here. \`onoff\` / \`level\` are in scope; the driver bus takes care of
 * telling every OTHER solution. */
static void apply_onoff_from_mesh(uint8_t onoff)
{
    (void)onoff;
${collectSlot(rendered, 'ble_mesh_onoff_set_cases', 4)}
}

static void apply_level_from_mesh(int16_t level)
{
    (void)level;
${collectSlot(rendered, 'ble_mesh_level_set_cases', 4)}
}

static void generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                              esp_ble_mesh_generic_server_cb_param_t *param)
{
    if (!param || event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT) {
        return;
    }
    switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:
            apply_onoff_from_mesh(param->value.state_change.onoff_set.onoff);
            break;
        case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET_UNACK:
            apply_level_from_mesh(param->value.state_change.level_set.level);
            break;
        case ESP_BLE_MESH_MODEL_OP_GEN_DELTA_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_DELTA_SET_UNACK:
            apply_level_from_mesh(param->value.state_change.delta_set.level);
            break;
        case ESP_BLE_MESH_MODEL_OP_GEN_MOVE_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_MOVE_SET_UNACK:
            apply_level_from_mesh(param->value.state_change.move_set.level);
            break;
        default:
            break;
    }
}

static void generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                              esp_ble_mesh_generic_client_cb_param_t *param)
{
    if (!param || !param->params) {
        return;
    }
    if (event == ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT) {
        ESP_LOGW(TAG, "no response to opcode 0x%04" PRIx32 " from 0x%04x",
                 param->params->opcode, param->params->ctx.addr);
        return;
    }
    ESP_LOGD(TAG, "generic client event %u, opcode 0x%04" PRIx32 ", err %d",
             event, param->params->opcode, param->error_code);
}

/* ── Hardware → mesh ───────────────────────────────────────────────────
 * Runs OFF the mesh task whenever any other solution or the hardware itself
 * changes a param. Device types either update their server state (an
 * actuator reporting itself) or publish through a client (a controller). */
static void ble_mesh_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)param_id;
    (void)val;
    (void)source;
    (void)ctx;
    if (!s_mesh_ready) return;
${collectSlot(rendered, 'ble_mesh_driver_cb_cases', 4)}
}

static void config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                             esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (!param || event != ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        return;
    }
    switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ESP_LOGI(TAG, "application key added (net_idx 0x%04x, app_idx 0x%04x)",
                     param->value.state_change.appkey_add.net_idx,
                     param->value.state_change.appkey_add.app_idx);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "model 0x%04x bound to app_idx 0x%04x",
                     param->value.state_change.mod_app_bind.model_id,
                     param->value.state_change.mod_app_bind.app_idx);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
            ESP_LOGI(TAG, "model 0x%04x subscribed to 0x%04x",
                     param->value.state_change.mod_sub_add.model_id,
                     param->value.state_change.mod_sub_add.sub_addr);
            break;
        default:
            break;
    }
}

/* Provisioning lifecycle. Behaviors contribute ble_mesh_prov_cases as plain
 * if-statements on \`event\` (with \`param\` in scope) — a provisioner block
 * handles its own PROVISIONER_* events there. */
static void provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                            esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
        case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
            ESP_LOGI(TAG, "waiting to be provisioned");
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
            ESP_LOGI(TAG, "provisioning link open (%s)",
                     param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
            ESP_LOGI(TAG, "provisioning link closed (%s)",
                     param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
            s_net_idx = param->node_prov_complete.net_idx;
            ESP_LOGI(TAG, "provisioned: address 0x%04x, net_idx 0x%04x",
                     param->node_prov_complete.addr, param->node_prov_complete.net_idx);
            break;
        case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
            /* Settings are erased by now. Go straight back to advertising so
             * the node can be re-added without a power cycle. */
            s_net_idx = ESP_BLE_MESH_KEY_PRIMARY;
            if (s_advertise_unprovisioned) {
                (void)esp_ble_mesh_node_prov_enable(ZC_MESH_BEARERS);
            }
            ESP_LOGW(TAG, "node reset — advertising as unprovisioned again");
            break;
        default:
            break;
    }
${collectSlot(rendered, 'ble_mesh_prov_cases', 4)}
}

/* ── Console ───────────────────────────────────────────────────────────
 * One command, because the questions a mesh device raises on a bench are
 * always the same five: is it in the network, get it in, get it out, and
 * make it say something. */
static int mesh_console_cmd(int argc, char **argv)
{
    const char *sub = (argc > 1) ? argv[1] : "status";
    if (!strcmp(sub, "status")) {
        bool joined = zc_ble_mesh_provisioned();
        printf("provisioned : %s\\n", joined ? "yes" : "no");
        printf("address     : 0x%04x\\n", esp_ble_mesh_get_primary_element_address());
        printf("net_idx     : 0x%04x\\n", s_net_idx);
        printf("onoff cli   : app_idx 0x%04x\\n", client_app_idx(&s_onoff_client));
        printf("level cli   : app_idx 0x%04x\\n", client_app_idx(&s_level_client));
        printf("group       : 0x%04x\\n", ZC_MESH_GROUP_ADDR);
        return 0;
    }
    if (!strcmp(sub, "join")) {
        return zc_ble_mesh_join_mode() == ESP_OK ? 0 : 1;
    }
    if (!strcmp(sub, "reset")) {
        return zc_ble_mesh_factory_reset() == ESP_OK ? 0 : 1;
    }
    if (!strcmp(sub, "on") || !strcmp(sub, "off")) {
        return zc_ble_mesh_publish_onoff(ZC_MESH_GROUP_ADDR, !strcmp(sub, "on")) == ESP_OK ? 0 : 1;
    }
    printf("usage: mesh <status|join|reset|on|off>\\n");
    return 1;
}

esp_err_t app_ble_mesh_init(void)
{
    s_handle = app_driver_register_solution("ble_mesh", ble_mesh_driver_cb, NULL);

    const esp_console_cmd_t mesh_cmd = {
        .command = "mesh",
        .help = "BLE Mesh: mesh <status|join|reset|on|off>",
        .hint = NULL,
        .func = &mesh_console_cmd,
        .argtable = NULL,
    };
    app_console_register_cmd(&mesh_cmd);

    esp_err_t err = ble_host_init();
    if (err != ESP_OK) {
        return err;
    }
    /* Marker stays in bytes 0-1; the rest is this device's own address, so no
     * two devices beacon the same UUID. */
    memcpy(s_dev_uuid + 2, s_bd_addr, sizeof(s_bd_addr));

    esp_ble_mesh_register_prov_callback(provisioning_cb);
    esp_ble_mesh_register_config_server_callback(config_server_cb);
    esp_ble_mesh_register_generic_server_callback(generic_server_cb);
    esp_ble_mesh_register_generic_client_callback(generic_client_cb);

    err = esp_ble_mesh_init(&s_provision, &s_composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize the mesh stack (%s)", esp_err_to_name(err));
        return err;
    }
    s_mesh_ready = true;

    /* Node-level features contributed by behavior blocks (the provisioner,
     * …) and each device type's initial state. */
${collectSlot(rendered, 'ble_mesh_node_init', 4)}

    if (zc_ble_mesh_provisioned()) {
        ESP_LOGI(TAG, "BLE Mesh node ready — already provisioned, address 0x%04x",
                 esp_ble_mesh_get_primary_element_address());
    } else if (s_advertise_unprovisioned) {
        err = esp_ble_mesh_node_prov_enable(ZC_MESH_BEARERS);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to advertise as unprovisioned (%s)", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "BLE Mesh node ready — unprovisioned, waiting to be added");
    } else {
        ESP_LOGI(TAG, "BLE Mesh ready — this device provisions the network");
    }
    return ESP_OK;
}
`
}

/**
 * app_sound.cpp — the Audio framework (id: sound): the device PLAYS sound.
 * A pattern sequencer over the param bus driving the product's existing
 * speaker_i2s tone driver — no second I2S owner; the speaker instance's
 * cfg.tone_param is baked in at generation. No speaker instance = the
 * framework compiles to a logged no-op (a silent product is a valid dev
 * state; a link error is not).
 */
function genAppSoundCpp(rendered: RenderedInstance[]): string {
  const speaker = rendered.find(r => r.block.id === 'drivers/speaker_i2s')
  const toneParam = speaker ? String((speaker.cfg as Record<string, unknown>)?.tone_param ?? '') : ''
  const body = '#include "app_sound.h"\n#include "app_driver.h"\n#include "app_config.h"\n\n#include <string.h>\n#include <esp_log.h>\n#include <freertos/FreeRTOS.h>\n#include <freertos/task.h>\n__SLOT_INCLUDES__\n\nstatic const char *TAG = "app_sound";\n\n#define ZC_SOUND_MAX_NOTES 24\nstatic zc_tone_t s_pattern[ZC_SOUND_MAX_NOTES];\nstatic volatile size_t s_count = 0;\nstatic volatile uint32_t s_generation = 0; /* bump = restart with new pattern */\nstatic TaskHandle_t s_task = NULL;\nstatic app_driver_handle_t s_handle = 0;\n\nesp_err_t app_sound_play(const zc_tone_t *pattern, size_t count)\n{\n__NO_SPEAKER_EARLY__\n    if (pattern == NULL || count == 0) return ESP_ERR_INVALID_ARG;\n    if (count > ZC_SOUND_MAX_NOTES) count = ZC_SOUND_MAX_NOTES;\n    /* Latest wins: alerts must not queue behind stale chimes. */\n    memcpy(s_pattern, pattern, count * sizeof(zc_tone_t));\n    s_count = count;\n    s_generation = s_generation + 1;\n    if (s_task) xTaskNotifyGive(s_task);\n    return ESP_OK;\n}\n\n__PLAYER_BODY__\n\n/* Device-type sound reactions ("what does this device sound like"). */\nstatic void sound_driver_cb(app_driver_param_id_t param_id, app_driver_param_val_t val, app_driver_handle_t source, void *arg)\n{\n    (void)source;\n    switch (param_id) {\n__SLOT_EVENT_CASES__\n        default: return;\n    }\n}\n\nesp_err_t app_sound_init(void)\n{\n    s_handle = app_driver_register_solution("sound", sound_driver_cb, NULL);\n__PLAYER_START__\n    ESP_LOGI(TAG, "Audio (sound) framework ready");\n    return ESP_OK;\n}'
  if (!toneParam) {
    return body
      .replace('__SLOT_INCLUDES__', collectSlot(rendered, 'sound_includes'))
      .replace('__NO_SPEAKER_EARLY__', '    static bool warned = false;\n    if (!warned) { warned = true; ESP_LOGW(TAG, "no speaker_i2s instance — sound is a no-op"); }\n    (void)pattern; (void)count;\n    return ESP_OK;')
      .replace('__PLAYER_BODY__', '/* No speaker instance: no sequencer. */')
      .replace('__PLAYER_START__', '')
      .replace('__SLOT_EVENT_CASES__', collectSlot(rendered, 'sound_event_cases', 8))
  }
  return body
    .replace('__SLOT_INCLUDES__', collectSlot(rendered, 'sound_includes'))
    .replace('__NO_SPEAKER_EARLY__', '')
    .replace('__PLAYER_BODY__', "/* Sequencer: steps the pattern into the speaker driver via its tone param\n * (__TONE_PARAM__ — baked from the product's speaker_i2s instance). The\n * driver owns I2S, volume and mute; this task only writes frequencies. */\nstatic void sound_player_task(void *arg)\n{\n    for (;;) {\n        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);\n        uint32_t gen = s_generation;\n        size_t n = s_count;\n        for (size_t i = 0; i < n && gen == s_generation; i++) {\n            app_driver_param_val_t v = { .u32 = s_pattern[i].freq_hz };\n            app_driver_set_param(__TONE_PARAM__, v, s_handle);\n            vTaskDelay(pdMS_TO_TICKS(s_pattern[i].ms ? s_pattern[i].ms : 1));\n        }\n        if (gen == s_generation) {\n            app_driver_param_val_t off = { .u32 = 0 };\n            app_driver_set_param(__TONE_PARAM__, off, s_handle);\n        }\n    }\n}".split('__TONE_PARAM__').join(toneParam))
    .replace('__PLAYER_START__', '    if (xTaskCreate(sound_player_task, "zc_sound", 3072, NULL, 5, &s_task) != pdPASS) {\n        ESP_LOGE(TAG, "player task create failed");\n        return ESP_FAIL;\n    }')
    .replace('__SLOT_EVENT_CASES__', collectSlot(rendered, 'sound_event_cases', 8))
}

/**
 * app_agents.cpp — the device is a client of the ESP Private Agents
 * platform. v1 is TEXT mode: websocket conversation + LOCAL TOOLS the cloud
 * agent invokes on this device (the binding: device types contribute
 * agents_statics handlers + agents_tool_register rows against the param
 * bus). Speech mode (their esp-sr/GMF audio path) is a deliberate later
 * deepening — it changes the flash budget class entirely.
 *
 * Standalone products own Wi-Fi via the console; with matter/rainmaker/mqtt
 * co-selected this owns NOTHING and rides the transport's network — the
 * same shared-net split mqtt and webui use.
 */
function genAppAgentsCpp(rendered: RenderedInstance[]): string {
  const sharedNet = rendered.some(r => isFramework(r, 'matter') || isFramework(r, 'rainmaker') || isFramework(r, 'mqtt'))
  const body = '#include "app_agents.h"\n#include "app_driver.h"\n#include "app_config.h"\n\n#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <esp_log.h>\n#include <esp_console.h>\n#include <esp_event.h>\n#include <esp_netif.h>\n#include <esp_wifi.h>\n#include <nvs.h>\n#include <esp_agent.h>\n#include <esp_agent_tools.h>\n#include <esp_agent_events.h>\n#include <esp_agent_messages.h>\n__SLOT_INCLUDES__\n\nstatic const char *TAG = "app_agents";\n\nstatic esp_agent_handle_t s_agent = NULL;\nstatic volatile bool s_started = false;\nstatic volatile bool s_have_ip = false;\nstatic char s_agent_id[64] = {0};\nstatic char s_token[300] = {0};\n__NET_STATICS__\n\n/* ── device-type tool handlers (param bus in/out) ─────────────────── */\n__SLOT_STATICS__\n\nstatic void agents_save_cfg(void)\n{\n    nvs_handle_t h;\n    if (nvs_open("zc_agents", NVS_READWRITE, &h) != ESP_OK) return;\n    nvs_set_str(h, "agent_id", s_agent_id);\n    nvs_set_str(h, "token", s_token);\n__NET_SAVE__\n    nvs_commit(h);\n    nvs_close(h);\n}\n\n/* Start once and only once everything is present. Never blocks boot: an\n * unconfigured device just logs what is missing. */\nstatic void agents_try_start(void)\n{\n    if (s_started || s_agent == NULL) return;\n    if (!s_have_ip) { ESP_LOGI(TAG, "waiting for network before starting"); return; }\n    if (s_agent_id[0] == 0 || s_token[0] == 0) {\n        ESP_LOGW(TAG, "not configured — set: agent-id <id>, agent-token <refresh token>");\n        return;\n    }\n    if (esp_agent_set_agent_id(s_agent, s_agent_id) != ESP_OK ||\n        esp_agent_set_refresh_token(s_agent, s_token) != ESP_OK) {\n        ESP_LOGE(TAG, "failed to set credentials");\n        return;\n    }\n    if (esp_agent_start(s_agent, NULL) == ESP_OK) {\n        s_started = true;\n        ESP_LOGI(TAG, "Agent conversation started (text mode)");\n    } else {\n        ESP_LOGE(TAG, "esp_agent_start failed — check credentials/network");\n    }\n}\n\n/* ── agent events ─────────────────────────────────────────────────── */\nstatic void agents_on_connected(void *arg, esp_event_base_t base, int32_t id, void *data)\n{\n    ESP_LOGI(TAG, "Agent connected");\n}\n\nstatic void agents_on_disconnected(void *arg, esp_event_base_t base, int32_t id, void *data)\n{\n    ESP_LOGW(TAG, "Agent disconnected — the client reconnects on its own");\n}\n\nstatic void agents_on_text(void *arg, esp_event_base_t base, int32_t id, void *data)\n{\n    esp_agent_message_data_t *msg = (esp_agent_message_data_t *)data;\n    if (msg == NULL || msg->text.text == NULL) return;\n    /* FINAL only: speculative chunks would interleave into console noise. */\n    if (msg->text.role == ESP_AGENT_MESSAGE_ROLE_ASSISTANT &&\n        msg->text.generation_stage != ESP_AGENT_MESSAGE_GENERATION_STAGE_SPECULATIVE) {\n        printf("\\n[agent] %s\\n", msg->text.text);\n    }\n}\n\nstatic void agents_on_ip(void *arg, esp_event_base_t base, int32_t id, void *data)\n{\n    s_have_ip = true;\n    ESP_LOGI(TAG, "network up");\n    agents_try_start();\n}\n\n/* ── console ──────────────────────────────────────────────────────── */\nstatic int agent_id_cmd(int argc, char **argv)\n{\n    if (argc < 2) { printf("usage: agent-id <id>\\n"); return 1; }\n    strlcpy(s_agent_id, argv[1], sizeof(s_agent_id));\n    agents_save_cfg();\n    printf("agent id saved\\n");\n    agents_try_start();\n    return 0;\n}\n\nstatic int agent_token_cmd(int argc, char **argv)\n{\n    if (argc < 2) { printf("usage: agent-token <refresh token>\\n"); return 1; }\n    strlcpy(s_token, argv[1], sizeof(s_token));\n    agents_save_cfg();\n    printf("token saved\\n");\n    agents_try_start();\n    return 0;\n}\n\nstatic int agent_say_cmd(int argc, char **argv)\n{\n    if (argc < 2) { printf("usage: agent-say <text…>\\n"); return 1; }\n    if (!s_started) { printf("agent not started — configure agent-id/agent-token first\\n"); return 1; }\n    char line[512] = {0};\n    for (int i = 1; i < argc; i++) {\n        if (i > 1) strlcat(line, " ", sizeof(line));\n        strlcat(line, argv[i], sizeof(line));\n    }\n    if (esp_agent_send_text(s_agent, line, pdMS_TO_TICKS(5000)) != ESP_OK) {\n        printf("send failed\\n");\n        return 1;\n    }\n    return 0;\n}\n\nstatic int agent_new_cmd(int argc, char **argv)\n{\n    if (!s_started) { printf("agent not started\\n"); return 1; }\n    return esp_agent_new_conversation(s_agent) == ESP_OK ? 0 : 1;\n}\n\nstatic int agent_status_cmd(int argc, char **argv)\n{\n    printf("network: %s\\nconfigured: %s\\nstarted: %s\\n",\n           s_have_ip ? "up" : "down",\n           (s_agent_id[0] && s_token[0]) ? "yes" : "no",\n           s_started ? "yes" : "no");\n    return 0;\n}\n__NET_CMD__\n\nesp_err_t app_agents_init(void)\n{\n    /* Restore config */\n    nvs_handle_t h;\n    if (nvs_open("zc_agents", NVS_READONLY, &h) == ESP_OK) {\n        size_t n = sizeof(s_agent_id); nvs_get_str(h, "agent_id", s_agent_id, &n);\n        n = sizeof(s_token); nvs_get_str(h, "token", s_token, &n);\n__NET_RESTORE__\n        nvs_close(h);\n    }\n\n    esp_agent_config_t cfg = {};\n    cfg.conversation_type = ESP_AGENT_CONVERSATION_TEXT;\n    /* UPSTREAM BUG WORKAROUND (esp-agents-firmware c5d6798, esp_agent.c:102):\n     * init dereferences upload/download_audio_config unconditionally — the\n     * NULL guard above it only covers SPEECH mode — so TEXT mode with NULL\n     * audio configs (the documented-valid case) is a Load access fault at\n     * boot. Values are copied by init, so stack locals are fine; they are\n     * unused in text mode. Drop when upstream fixes the guard. */\n    esp_agent_audio_config_t audio_dummy = {};\n    audio_dummy.format = ESP_AGENT_CONVERSATION_AUDIO_FORMAT_PCM;\n    audio_dummy.sample_rate = 16000;\n    audio_dummy.frame_duration = 20;\n    cfg.upload_audio_config = &audio_dummy;\n    cfg.download_audio_config = &audio_dummy;\n    s_agent = esp_agent_init(&cfg);\n    if (s_agent == NULL) {\n        ESP_LOGE(TAG, "esp_agent_init failed");\n        return ESP_FAIL;\n    }\n    esp_agent_register_event_handler(s_agent, ESP_AGENT_EVENT_CONNECTED, agents_on_connected, NULL, NULL);\n    esp_agent_register_event_handler(s_agent, ESP_AGENT_EVENT_DISCONNECTED, agents_on_disconnected, NULL, NULL);\n    esp_agent_register_event_handler(s_agent, ESP_AGENT_EVENT_DATA_TYPE_TEXT, agents_on_text, NULL, NULL);\n\n    /* Local tools the cloud agent may invoke on THIS device. */\n__SLOT_TOOL_REGISTER__\n\n    const esp_console_cmd_t cmds[] = {\n        { .command = "agent-id", .help = "agent-id <id> — set the agent to talk to", .func = &agent_id_cmd },\n        { .command = "agent-token", .help = "agent-token <refresh token> — from the Agents platform UI", .func = &agent_token_cmd },\n        { .command = "agent-say", .help = "agent-say <text> — send a message to the agent", .func = &agent_say_cmd },\n        { .command = "agent-new", .help = "start a fresh conversation", .func = &agent_new_cmd },\n        { .command = "agent-status", .help = "connection/config state", .func = &agent_status_cmd },\n    };\n    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {\n        esp_console_cmd_register(&cmds[i]);\n    }\n__NET_INIT__\n\n    ESP_LOGI(TAG, "Agents client ready (text mode)");\n    return ESP_OK;\n}\n'
  return body
    .replace('__SLOT_INCLUDES__', collectSlot(rendered, 'agents_includes'))
    .replace('__SLOT_STATICS__', collectSlot(rendered, 'agents_statics'))
    .replace('__SLOT_TOOL_REGISTER__', collectSlot(rendered, 'agents_tool_register', 4))
    .replace('__NET_STATICS__', sharedNet ? '' : 'static char s_ssid[33] = {0};\nstatic char s_pass[65] = {0};')
    .replace('__NET_SAVE__', sharedNet ? '' : '    nvs_set_str(h, "ssid", s_ssid);\n    nvs_set_str(h, "pass", s_pass);')
    .replace('__NET_RESTORE__', sharedNet ? '' : '        n = sizeof(s_ssid); nvs_get_str(h, "ssid", s_ssid, &n);\n        n = sizeof(s_pass); nvs_get_str(h, "pass", s_pass, &n);')
    .replace('__NET_CMD__', sharedNet ? '' : '\n\nstatic void agents_wifi_connect(void)\n{\n    if (s_ssid[0] == 0) { ESP_LOGW(TAG, "no Wi-Fi configured — agent-wifi <ssid> <pass>"); return; }\n    wifi_config_t wc = {};\n    strlcpy((char *)wc.sta.ssid, s_ssid, sizeof(wc.sta.ssid));\n    strlcpy((char *)wc.sta.password, s_pass, sizeof(wc.sta.password));\n    esp_wifi_set_mode(WIFI_MODE_STA);\n    esp_wifi_set_config(WIFI_IF_STA, &wc);\n    esp_wifi_connect();\n}\n\nstatic void agents_on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)\n{\n    if (id == WIFI_EVENT_STA_START) {\n        agents_wifi_connect();\n    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {\n        s_have_ip = false;\n        esp_wifi_connect(); /* simple retry — esp-wifi rate-limits internally */\n    }\n}\n\nstatic int agent_wifi_cmd(int argc, char **argv)\n{\n    if (argc < 3) { printf("usage: agent-wifi <ssid> <password>\\n"); return 1; }\n    strlcpy(s_ssid, argv[1], sizeof(s_ssid));\n    strlcpy(s_pass, argv[2], sizeof(s_pass));\n    agents_save_cfg();\n    printf("wifi saved — connecting\\n");\n    agents_wifi_connect();\n    return 0;\n}')
    .replace('__NET_INIT__', sharedNet ? '\n    /* Shared-network mode: a co-selected transport owns Wi-Fi. Watch for its\n     * IP (and handle the already-connected case on reboot). */\n    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, agents_on_ip, NULL);\n    esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, agents_on_ip, NULL);\n#if CONFIG_LWIP_IPV4\n    {\n        esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");\n        esp_netif_ip_info_t ip;\n        if (sta && esp_netif_get_ip_info(sta, &ip) == ESP_OK && ip.ip.addr != 0) {\n            s_have_ip = true;\n        }\n    }\n#endif\n    agents_try_start();' : '\n    /* Standalone: this framework owns Wi-Fi, console-provisioned (mqtt\'s\n     * pattern — a bare agents node has no phone-app provisioning). */\n    ESP_ERROR_CHECK(esp_netif_init());\n    esp_err_t loop_err = esp_event_loop_create_default();\n    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(loop_err);\n    esp_netif_create_default_wifi_sta();\n    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();\n    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));\n    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, agents_on_wifi, NULL);\n    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, agents_on_ip, NULL);\n    ESP_ERROR_CHECK(esp_wifi_start());\n    const esp_console_cmd_t wifi_cmd = {\n        .command = "agent-wifi", .help = "agent-wifi <ssid> <pass>", .func = &agent_wifi_cmd,\n    };\n    esp_console_cmd_register(&wifi_cmd);')
}

/**
 * app_webui.cpp — the device serves its own control page.
 *
 * Binding: device types contribute webui_entities manifest rows; ONE
 * embedded page renders them by kind (toggle/slider/value/binary) against
 * GET /api/state + GET /api/set. Standalone products bring up a SoftAP;
 * with a Wi-Fi transport co-selected, webui owns nothing and serves on the
 * transport's network (+ mDNS either way).
 *
 * The embedded page is plain-ASCII vanilla JS: no backticks, no dollar-
 * brace, no double braces — it must survive the TS template literal AND
 * the slot engine.
 */
function genAppWebuiCpp(rendered: RenderedInstance[]): string {
  // An empty manifest is a DEGENERATE page: the server comes up and serves a
  // control page with no controls, which reads as "working" and is not.
  // Observed live on hallway-web-panel-msmz4sqr — smart_plug had no
  // webui_entities slot, s_entities[] rendered empty, and code_writing quietly
  // replaced the whole generated component with a hand-rolled one. Fail here
  // instead, naming the device types that need the binding.
  const entities = collectSlot(rendered, 'webui_entities', 4)
  if (!entities.trim()) {
    const deviceTypes = rendered
      .map(r => r.block.id)
      .filter(id => id.startsWith('device_types/'))
    throw new Error(
      'frameworks: [webui] but no device type contributes a webui_entities row, ' +
      'so the control page would have no controls. ' +
      (deviceTypes.length
        ? `Add a slots/webui_entities.c to: ${deviceTypes.join(', ')}.`
        : 'Add at least one device_types/* instance to the product.'),
    )
  }
  const sharedNet = rendered.some(r => isFramework(r, 'matter') || isFramework(r, 'rainmaker') || isFramework(r, 'mqtt'))
  const body = '#include "app_webui.h"\n#include "app_driver.h"\n#include "app_config.h"\n\n#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <esp_log.h>\n#include <esp_event.h>\n#include <esp_netif.h>\n#include <esp_wifi.h>\n#include <esp_mac.h>\n#include <esp_http_server.h>\n#include <mdns.h>\n__SLOT_INCLUDES__\n\nstatic const char *TAG = "app_webui";\n\n/* vtype: 0 bool, 1 u8, 2 i16, 3 u16, 4 u32 (shared tag vocabulary). */\ntypedef struct {\n    const char *label;\n    const char *kind;   /* toggle | slider | value | binary */\n    app_driver_param_id_t param;\n    const char *param_name;\n    uint8_t vtype;\n    int32_t min;\n    int32_t max;\n    int32_t scale;      /* UI divides the raw value by this for display */\n} zc_webui_entity_t;\n\nstatic const zc_webui_entity_t s_entities[] = {\n__SLOT_ENTITIES__\n};\n#define ZC_WEBUI_N (sizeof(s_entities) / sizeof(s_entities[0]))\n\n/* The whole UI: plain-ASCII vanilla JS, no external assets. DOM rebuilds on\n * each poll unless a control is focused (mid-drag). */\nstatic const char INDEX_HTML[] =\n"<!DOCTYPE html><html><head><meta name=viewport content=\'width=device-width,initial-scale=1\'>"\n"<title>ZeroCode</title><style>"\n"body{font-family:system-ui;margin:0;background:#141414;color:#eee}"\n"h1{font-size:17px;padding:14px 16px;margin:0;border-bottom:1px solid #333;color:#e0a06a}"\n".c{padding:14px 16px;border-bottom:1px solid #262626;display:flex;justify-content:space-between;align-items:center;gap:12px}"\n".v{color:#7fd4a3;font-variant-numeric:tabular-nums}"\n".on{color:#7fd4a3}.off{color:#888}"\n"input[type=range]{width:46vw}"\n"</style></head><body><h1>ZeroCode</h1><div id=root></div><script>"\n"function post(p,v){fetch(\'/api/set?param=\'+encodeURIComponent(p)+\'&value=\'+v);}"\n"function row(e){"\n"var d=document.createElement(\'div\');d.className=\'c\';"\n"var l=document.createElement(\'span\');l.textContent=e.label;d.appendChild(l);"\n"if(e.kind==\'toggle\'){var i=document.createElement(\'input\');i.type=\'checkbox\';i.checked=e.value!=0;"\n"i.onchange=function(){post(e.param,i.checked?1:0);};d.appendChild(i);}"\n"else if(e.kind==\'slider\'){var w=document.createElement(\'span\');"\n"var i=document.createElement(\'input\');i.type=\'range\';i.min=e.min;i.max=e.max;i.value=e.value;"\n"i.onchange=function(){post(e.param,i.value);};w.appendChild(i);d.appendChild(w);}"\n"else if(e.kind==\'binary\'){var s=document.createElement(\'span\');"\n"s.textContent=e.value!=0?\'ACTIVE\':\'clear\';s.className=e.value!=0?\'on\':\'off\';d.appendChild(s);}"\n"else{var s=document.createElement(\'span\');s.className=\'v\';"\n"s.textContent=(e.scale>1?(e.value/e.scale).toFixed(2):e.value);d.appendChild(s);}"\n"return d;}"\n"function render(st){if(document.activeElement&&document.activeElement.tagName==\'INPUT\')return;"\n"var r=document.getElementById(\'root\');r.textContent=\'\';"\n"st.entities.forEach(function(e){r.appendChild(row(e));});}"\n"function tick(){fetch(\'/api/state\').then(function(r){return r.json();}).then(render).catch(function(){});}"\n"tick();setInterval(tick,2500);"\n"</script></body></html>";\n\nstatic int32_t entity_value(const zc_webui_entity_t *e)\n{\n    app_driver_param_val_t v = {};\n    app_driver_get_param(e->param, &v);\n    switch (e->vtype) {\n        case 0: return v.b ? 1 : 0;\n        case 1: return v.u8;\n        case 2: return v.i16;\n        case 3: return v.u16;\n        default: return (int32_t)v.u32;\n    }\n}\n\nstatic esp_err_t index_get(httpd_req_t *req)\n{\n    httpd_resp_set_type(req, "text/html");\n    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);\n}\n\nstatic esp_err_t state_get(httpd_req_t *req)\n{\n    httpd_resp_set_type(req, "application/json");\n    httpd_resp_sendstr_chunk(req, "{\\"entities\\":[");\n    char buf[240];\n    for (size_t i = 0; i < ZC_WEBUI_N; i++) {\n        const zc_webui_entity_t *e = &s_entities[i];\n        snprintf(buf, sizeof(buf),\n                 "%s{\\"label\\":\\"%s\\",\\"kind\\":\\"%s\\",\\"param\\":\\"%s\\","\n                 "\\"min\\":%ld,\\"max\\":%ld,\\"scale\\":%ld,\\"value\\":%ld}",\n                 i ? "," : "", e->label, e->kind, e->param_name,\n                 (long)e->min, (long)e->max, (long)e->scale, (long)entity_value(e));\n        httpd_resp_sendstr_chunk(req, buf);\n    }\n    httpd_resp_sendstr_chunk(req, "]}");\n    httpd_resp_sendstr_chunk(req, NULL);\n    return ESP_OK;\n}\n\nstatic esp_err_t set_get(httpd_req_t *req)\n{\n    char query[128] = {0};\n    char pname[64] = {0};\n    char valstr[16] = {0};\n    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||\n        httpd_query_key_value(query, "param", pname, sizeof(pname)) != ESP_OK ||\n        httpd_query_key_value(query, "value", valstr, sizeof(valstr)) != ESP_OK) {\n        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "param and value required");\n        return ESP_FAIL;\n    }\n    long raw = strtol(valstr, NULL, 10);\n    for (size_t i = 0; i < ZC_WEBUI_N; i++) {\n        const zc_webui_entity_t *e = &s_entities[i];\n        if (strcmp(e->param_name, pname) != 0) continue;\n        if (raw < e->min) raw = e->min;\n        if (raw > e->max) raw = e->max;\n        app_driver_param_val_t v = {};\n        switch (e->vtype) {\n            case 0: v.b = raw != 0; break;\n            case 1: v.u8 = (uint8_t)raw; break;\n            case 2: v.i16 = (int16_t)raw; break;\n            case 3: v.u16 = (uint16_t)raw; break;\n            default: v.u32 = (uint32_t)raw; break;\n        }\n        app_driver_set_param(e->param, v, APP_DRIVER_SOURCE_LOCAL);\n        httpd_resp_sendstr(req, "ok");\n        return ESP_OK;\n    }\n    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "unknown param");\n    return ESP_FAIL;\n}\n\nstatic void webui_start_server(void)\n{\n    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();\n    cfg.lru_purge_enable = true;\n    httpd_handle_t server = NULL;\n    if (httpd_start(&server, &cfg) != ESP_OK) {\n        ESP_LOGE(TAG, "httpd start failed");\n        return;\n    }\n    static const httpd_uri_t u_index = { .uri = "/", .method = HTTP_GET, .handler = index_get, .user_ctx = NULL };\n    static const httpd_uri_t u_state = { .uri = "/api/state", .method = HTTP_GET, .handler = state_get, .user_ctx = NULL };\n    static const httpd_uri_t u_set   = { .uri = "/api/set", .method = HTTP_GET, .handler = set_get, .user_ctx = NULL };\n    httpd_register_uri_handler(server, &u_index);\n    httpd_register_uri_handler(server, &u_state);\n    httpd_register_uri_handler(server, &u_set);\n}\n\nesp_err_t app_webui_init(void)\n{\n    uint8_t mac[6] = {0};\n    esp_read_mac(mac, ESP_MAC_WIFI_STA);\n    char host[24];\n    snprintf(host, sizeof(host), "zerocode-%02x%02x", mac[4], mac[5]);\n\n__NET_SETUP__\n\n    if (mdns_init() == ESP_OK) {\n        mdns_hostname_set(host);\n        mdns_instance_name_set("ZeroCode device");\n        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);\n    }\n\n    webui_start_server();\n__SLOT_NODE_INIT__\n\n    ESP_LOGI(TAG, "Web UI ready — http://%s.local/", host);\n    return ESP_OK;\n}\n'
  const netSetup = sharedNet ? '    /* Shared-network mode: a Wi-Fi transport owns the network; the server\n     * just binds on it. Nothing to bring up here. */' : '    /* Standalone: bring up a SoftAP — the device IS the network. */\n    ESP_ERROR_CHECK(esp_netif_init());\n    esp_err_t loop_err = esp_event_loop_create_default();\n    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {\n        ESP_ERROR_CHECK(loop_err);\n    }\n    esp_netif_create_default_wifi_ap();\n    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();\n    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));\n    wifi_config_t ap = {};\n    snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "ZeroCode-%02X%02X", mac[4], mac[5]);\n    strlcpy((char *)ap.ap.password, "zerocode", sizeof(ap.ap.password));\n    ap.ap.ssid_len = 0;\n    ap.ap.channel = 1;\n    ap.ap.max_connection = 4;\n    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;\n    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));\n    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));\n    ESP_ERROR_CHECK(esp_wifi_start());\n    ESP_LOGI(TAG, "SoftAP \'%s\' up — connect and open http://192.168.4.1/", (char *)ap.ap.ssid);'
  return body
    .replace('__SLOT_INCLUDES__', collectSlot(rendered, 'webui_includes'))
    .replace('__SLOT_ENTITIES__', entities)
    .replace('__NET_SETUP__', netSetup)
    .replace('__SLOT_NODE_INIT__', collectSlot(rendered, 'webui_node_init', 4))
}

/**
 * app_espnow.cpp — the device-to-device framework.
 *
 * Binding: "what does this device say to its peers." Broadcaster device
 * types map param changes to zc_espnow_send(name, type, value) via
 * espnow_emit_cases; listeners match inbound name-hashes to their local
 * params via espnow_recv_cases. Names are the kit vocabulary — the wire
 * carries an FNV-1a hash of the param NAME, so separately built products
 * interoperate without coordinating numeric enums.
 *
 * v1 trust model: open broadcast + group-id filter, channel 1, no
 * encryption (ESP-NOW cannot encrypt broadcast frames). Trusted spaces
 * only; pairing + per-peer LMK is v2.
 */
function genAppEspnowCpp(rendered: RenderedInstance[]): string {
  return `#include "app_espnow.h"
#include "app_driver.h"
#include "app_config.h"

#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_timer.h>
#include <esp_console.h>
#include <nvs.h>
#include <nvs_flash.h>
${collectSlot(rendered, 'espnow_includes')}

static const char *TAG = "app_espnow";

static app_driver_handle_t s_handle = 0;

/* Wire format: 14 packed bytes. Value types: 0 bool, 1 u8, 2 i16, 3 u16, 4 u32.
 * Types >= 0xF0 are control frames (pairing) and never reach the device-type
 * cases. */
#define ZC_NOW_MAGIC 0x5A434E57u /* "ZCNW" */
#define ZC_NOW_GROUP 0           /* group-id filter; per-product cfg is v2 */
#define ZC_NOW_T_PAIR_REQ 0xF0
#define ZC_NOW_T_PAIR_ACK 0xF1
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  group;
    uint8_t  type;
    uint32_t name_hash;
    uint32_t value;
} zc_now_msg_t;

static const uint8_t ZC_NOW_BCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* ── Peers and pairing ────────────────────────────────────────────────────
 * A kit with NO stored peers is the open v1 kit: broadcast out, accept every
 * matching frame in. Once a device holds peers it talks to THEM: sends go
 * unicast to each, frames from any other radio are dropped. Peers are learnt
 * in a pairing window both sides open (a button, a console command): the
 * requester broadcasts PAIR_REQ while its window is open, an acceptor with
 * its window open answers PAIR_ACK and stores the requester, the requester
 * stores the acceptor on the ACK. Stored in NVS, so pairing survives a power
 * cycle. This is the peer side of the framework's v2 story; per-peer
 * encryption (LMK) is the next step and does not change this contract. */
#define ZC_NOW_MAX_PEERS 8
#define ZC_NOW_NVS_NS   "zc_now"
#define ZC_NOW_NVS_KEY  "peers"
static uint8_t s_peers[ZC_NOW_MAX_PEERS][6];
static uint8_t s_peer_count = 0;
static volatile bool s_pairing = false;
static esp_timer_handle_t s_pair_timer = NULL;   /* window end */
static esp_timer_handle_t s_pair_beacon = NULL;  /* PAIR_REQ repeat */

static bool zc_now_peer_known(const uint8_t *mac)
{
    for (uint8_t i = 0; i < s_peer_count; i++) {
        if (memcmp(s_peers[i], mac, 6) == 0) return true;
    }
    return false;
}

static void zc_now_peers_save(void)
{
    nvs_handle_t h;
    if (nvs_open(ZC_NOW_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, ZC_NOW_NVS_KEY, s_peers, (size_t)s_peer_count * 6);
    nvs_commit(h);
    nvs_close(h);
}

static void zc_now_peers_load(void)
{
    nvs_handle_t h;
    if (nvs_open(ZC_NOW_NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(s_peers);
    if (nvs_get_blob(h, ZC_NOW_NVS_KEY, s_peers, &len) == ESP_OK) {
        s_peer_count = (uint8_t)(len / 6);
    }
    nvs_close(h);
}

/* The radio needs a peer entry before esp_now_send can unicast to it. */
static void zc_now_radio_add_peer(const uint8_t *mac)
{
    if (esp_now_is_peer_exist(mac)) return;
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK) ESP_LOGW(TAG, "add peer failed: %s", esp_err_to_name(err));
}

static bool zc_now_peer_store(const uint8_t *mac)
{
    if (zc_now_peer_known(mac)) return true;
    if (s_peer_count >= ZC_NOW_MAX_PEERS) {
        ESP_LOGW(TAG, "peer table full (%d) — forget a peer first", ZC_NOW_MAX_PEERS);
        return false;
    }
    memcpy(s_peers[s_peer_count++], mac, 6);
    zc_now_radio_add_peer(mac);
    zc_now_peers_save();
    ESP_LOGI(TAG, "paired with %02x:%02x:%02x:%02x:%02x:%02x (%d peer%s)",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], s_peer_count, s_peer_count == 1 ? "" : "s");
    return true;
}

static void zc_now_send_raw(const uint8_t *mac, uint8_t type, uint32_t name_hash, uint32_t value)
{
    zc_now_msg_t msg = {};
    msg.magic = ZC_NOW_MAGIC;
    msg.group = ZC_NOW_GROUP;
    msg.type = type;
    msg.name_hash = name_hash;
    msg.value = value;
    esp_err_t err = esp_now_send(mac, (const uint8_t *)&msg, sizeof(msg));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "send failed: %s", esp_err_to_name(err));
    }
}

static void zc_now_pair_beacon_cb(void *arg)
{
    (void)arg;
    if (!s_pairing) return;
    zc_now_send_raw(ZC_NOW_BCAST, ZC_NOW_T_PAIR_REQ, 0, 0);
}

static void zc_now_pair_end_cb(void *arg)
{
    (void)arg;
    s_pairing = false;
    (void)esp_timer_stop(s_pair_beacon);
    ESP_LOGI(TAG, "pairing window closed (%d peer%s)", s_peer_count, s_peer_count == 1 ? "" : "s");
}

void zc_espnow_pair_start(uint32_t window_ms)
{
    if (window_ms == 0) window_ms = 30000;
    s_pairing = true;
    (void)esp_timer_stop(s_pair_timer);
    (void)esp_timer_stop(s_pair_beacon);
    (void)esp_timer_start_once(s_pair_timer, (uint64_t)window_ms * 1000ULL);
    (void)esp_timer_start_periodic(s_pair_beacon, 500 * 1000ULL);
    ESP_LOGI(TAG, "pairing window open for %lu ms — open it on the other device too",
             (unsigned long)window_ms);
}

bool zc_espnow_pairing(void) { return s_pairing; }
uint8_t zc_espnow_peer_count(void) { return s_peer_count; }

void zc_espnow_forget_peers(void)
{
    for (uint8_t i = 0; i < s_peer_count; i++) (void)esp_now_del_peer(s_peers[i]);
    s_peer_count = 0;
    zc_now_peers_save();
    ESP_LOGI(TAG, "peers forgotten — back to open broadcast");
}

/* Bench commands. Both sides of a kit need their window open at once, so a
 * console on each is the simplest way to pair two boards on a desk. */
static int zc_now_cmd_pair(int argc, char **argv)
{
    uint32_t ms = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) * 1000UL : 30000UL;
    zc_espnow_pair_start(ms);
    return 0;
}
static int zc_now_cmd_peers(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("%d peer(s)%s\\n", s_peer_count, s_peer_count ? "" : " (open kit: broadcast)");
    for (uint8_t i = 0; i < s_peer_count; i++) {
        printf("  %02x:%02x:%02x:%02x:%02x:%02x\\n", s_peers[i][0], s_peers[i][1], s_peers[i][2],
               s_peers[i][3], s_peers[i][4], s_peers[i][5]);
    }
    return 0;
}
static int zc_now_cmd_forget(int argc, char **argv)
{
    (void)argc; (void)argv;
    zc_espnow_forget_peers();
    return 0;
}

/* FNV-1a over the param NAME — the cross-product kit vocabulary. */
static uint32_t zc_now_hash(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

${collectSlot(rendered, 'espnow_statics')}

void zc_espnow_send(const char *param_name, uint8_t type, uint32_t value)
{
    uint32_t hash = zc_now_hash(param_name);
    if (s_peer_count == 0) {
        zc_now_send_raw(ZC_NOW_BCAST, type, hash, value);
        return;
    }
    for (uint8_t i = 0; i < s_peer_count; i++) {
        zc_now_send_raw(s_peers[i], type, hash, value);
    }
}

/* Peers' broadcasts -> local param bus. Listener device types own the cases
 * (hash + type in scope). Runs on the Wi-Fi task — set_param is safe (its
 * own critical section), keep cases short. */
static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len)
{
    if (data_len != (int)sizeof(zc_now_msg_t)) return;
    zc_now_msg_t msg;
    memcpy(&msg, data, sizeof(msg));
    if (msg.magic != ZC_NOW_MAGIC || msg.group != ZC_NOW_GROUP) return;
    const uint8_t *from = info ? info->src_addr : NULL;
    if (msg.type >= 0xF0) {
        /* Pairing. Only while our own window is open — a stray request from a
           neighbour's kit is ignored, which is the whole point of pairing. */
        if (!from || !s_pairing) return;
        if (msg.type == ZC_NOW_T_PAIR_REQ) {
            if (zc_now_peer_store(from)) zc_now_send_raw(from, ZC_NOW_T_PAIR_ACK, 0, 0);
        } else if (msg.type == ZC_NOW_T_PAIR_ACK) {
            (void)zc_now_peer_store(from);
        }
        return;
    }
    if (s_peer_count > 0 && (!from || !zc_now_peer_known(from))) return;
    uint32_t hash = msg.name_hash;
    (void)hash;
${collectSlot(rendered, 'espnow_recv_cases', 4)}
}

/* Hardware/param bus -> broadcasts. Broadcaster device types own the cases. */
static void espnow_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
    (void)val;
    switch (param_id) {
${collectSlot(rendered, 'espnow_driver_cb_cases', 4)}
    default:
        break;
    }
}

esp_err_t app_espnow_init(void)
{
    s_handle = app_driver_register_solution("espnow", espnow_driver_cb, NULL);

    /* Wi-Fi up in STA mode, NOT connected — ESP-NOW rides the radio alone.
     * All kit members pin channel 1 (see the trust-model note). */
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(loop_err);
    }
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, ZC_NOW_BCAST, sizeof(peer.peer_addr));
    peer.channel = 0; /* current channel */
    peer.ifidx = WIFI_IF_STA;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    /* Peers learnt in an earlier pairing window, and the window's timers. */
    zc_now_peers_load();
    for (uint8_t i = 0; i < s_peer_count; i++) zc_now_radio_add_peer(s_peers[i]);
    const esp_timer_create_args_t end_args = {
        .callback = &zc_now_pair_end_cb, .arg = NULL, .dispatch_method = ESP_TIMER_TASK,
        .name = "now_pair_end", .skip_unhandled_events = true,
    };
    const esp_timer_create_args_t beacon_args = {
        .callback = &zc_now_pair_beacon_cb, .arg = NULL, .dispatch_method = ESP_TIMER_TASK,
        .name = "now_pair_req", .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&end_args, &s_pair_timer));
    ESP_ERROR_CHECK(esp_timer_create(&beacon_args, &s_pair_beacon));
    {
        esp_console_cmd_t c = {};
        c.command = "espnow-pair"; c.help = "espnow-pair [seconds] — open the pairing window (default 30 s)"; c.func = &zc_now_cmd_pair;
        esp_console_cmd_register(&c);
        c = {};
        c.command = "espnow-peers"; c.help = "list paired peers"; c.func = &zc_now_cmd_peers;
        esp_console_cmd_register(&c);
        c = {};
        c.command = "espnow-forget"; c.help = "forget every paired peer (back to open broadcast)"; c.func = &zc_now_cmd_forget;
        esp_console_cmd_register(&c);
    }

    /* Node-level contributions (behaviors). */
${collectSlot(rendered, 'espnow_node_init', 4)}

    ESP_LOGI(TAG, "ESP-NOW initialized (channel 1, group %d, %d paired peer%s)",
             ZC_NOW_GROUP, s_peer_count, s_peer_count == 1 ? "" : "s");
    return ESP_OK;
}
`
}

/**
 * app_ble_hid.cpp — the BLE media-remote framework.
 *
 * The binding inverts the other way: not "how does this device appear" but
 * "what does this device SEND". Controller device types map param-bus
 * changes to HID consumer usages via ble_hid_driver_cb_cases, using
 * zc_ble_hid_send_consumer(usage) (press + release, report id 1).
 *
 * GAP/advertising/security boilerplate lives in the component's
 * ble_hid_gap.c (BLE-only NimBLE, adapted from the IDF example) — this
 * generated file owns the report map, the esp_hidd device, and dispatch.
 */
function genAppBleHidCpp(rendered: RenderedInstance[]): string {
  return `#include "app_ble_hid.h"
#include "ble_hid_gap.h"
#include "app_driver.h"
#include "app_config.h"

#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_hidd.h>
${collectSlot(rendered, 'ble_hid_includes')}

static const char *TAG = "app_ble_hid";

static app_driver_handle_t s_handle = 0;
static esp_hidd_dev_t *s_hid_dev = NULL;
static volatile bool s_host_connected = false;

/* Consumer Control, report id 1: one 16-bit usage array. Press = usage,
 * release = 0. Covers play/pause, volume, mute, next/prev — everything a
 * media remote sends. */
static const uint8_t s_report_map[] = {
    0x05, 0x0C,       /* Usage Page (Consumer) */
    0x09, 0x01,       /* Usage (Consumer Control) */
    0xA1, 0x01,       /* Collection (Application) */
    0x85, 0x01,       /*   Report ID (1) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x26, 0xFF, 0x03, /*   Logical Maximum (0x3FF) */
    0x19, 0x00,       /*   Usage Minimum (0) */
    0x2A, 0xFF, 0x03, /*   Usage Maximum (0x3FF) */
    0x75, 0x10,       /*   Report Size (16) */
    0x95, 0x01,       /*   Report Count (1) */
    0x81, 0x00,       /*   Input (Data, Array) */
    0xC0,             /* End Collection */
};

${collectSlot(rendered, 'ble_hid_statics')}

/* Press + release. Dropped (with a log) when no host is connected — a
 * remote pressed before pairing is a no-op, not an error. */
void zc_ble_hid_send_consumer(uint16_t usage)
{
    if (!s_host_connected || s_hid_dev == NULL) {
        ESP_LOGD(TAG, "no host connected — dropping usage 0x%03x", usage);
        return;
    }
    uint8_t report[2] = { (uint8_t)(usage & 0xff), (uint8_t)(usage >> 8) };
    esp_hidd_dev_input_set(s_hid_dev, 0, 1, report, 2);
    uint8_t release[2] = { 0, 0 };
    esp_hidd_dev_input_set(s_hid_dev, 0, 1, release, 2);
}

/* Hardware/param bus -> HID reports. Device types own the cases. */
static void ble_hid_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
    (void)val;
    switch (param_id) {
${collectSlot(rendered, 'ble_hid_driver_cb_cases', 4)}
    default:
        break;
    }
}

static void hidd_event_callback(void *args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)args; (void)base;
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    switch (event) {
        case ESP_HIDD_START_EVENT:
            ESP_LOGI(TAG, "HID device started — advertising");
            ble_hid_gap_adv_start();
            break;
        case ESP_HIDD_CONNECT_EVENT:
            s_host_connected = true;
            ESP_LOGI(TAG, "HID host connected");
            break;
        case ESP_HIDD_DISCONNECT_EVENT:
            s_host_connected = false;
            ESP_LOGI(TAG, "HID host disconnected");
            break;
        case ESP_HIDD_PROTOCOL_MODE_EVENT:
            ESP_LOGI(TAG, "protocol mode: %s", param->protocol_mode.protocol_mode ? "report" : "boot");
            break;
        default:
            break;
    }
}

esp_err_t app_ble_hid_init(void)
{
    s_handle = app_driver_register_solution("ble_hid", ble_hid_driver_cb, NULL);

    ESP_ERROR_CHECK(ble_hid_gap_stack_init());
    ESP_ERROR_CHECK(ble_hid_gap_adv_init(0x03C0 /* generic HID */, "ZeroCode Remote"));

    static esp_hid_raw_report_map_t report_maps[] = {
        { .data = s_report_map, .len = sizeof(s_report_map) },
    };
    static esp_hid_device_config_t hid_config = {};
    hid_config.vendor_id = 0x16A8;
    hid_config.product_id = 0x2C0D;
    hid_config.version = 0x0100;
    hid_config.device_name = "ZeroCode Remote";
    hid_config.manufacturer_name = "ZeroCode AI";
    hid_config.serial_number = "0";
    hid_config.report_maps = report_maps;
    hid_config.report_maps_len = 1;
    ESP_ERROR_CHECK(esp_hidd_dev_init(&hid_config, ESP_HID_TRANSPORT_BLE,
                                      hidd_event_callback, &s_hid_dev));

    /* Node-level contributions (behaviors) before the host task starts. */
${collectSlot(rendered, 'ble_hid_node_init', 4)}

    ESP_ERROR_CHECK(ble_hid_gap_start_host());

    ESP_LOGI(TAG, "BLE HID initialized — pair from your phone/PC Bluetooth settings");
    return ESP_OK;
}
`
}

/**
 * app_vision.cpp — the camera perception framework.
 *
 * The lightest framework spine: detector DRIVER blocks (camera + algorithm)
 * own the hardware and their capture/detect tasks, and report through
 * zc_vision_emit(event_id, value); this file dispatches those events into
 * device-type / behavior slots (vision_event_cases) which drive the param
 * bus. Detection stays in blocks so new detectors (esp-who person/face)
 * extend the event vocabulary without generator changes.
 */
function genAppVisionCpp(rendered: RenderedInstance[]): string {
  return `#include "app_vision.h"
#include "app_driver.h"
#include "app_config.h"

#include <esp_log.h>
${collectSlot(rendered, 'vision_includes')}

static const char *TAG = "app_vision";

static app_driver_handle_t s_handle = 0;

${collectSlot(rendered, 'vision_statics')}

/* Detector blocks report here (from their own tasks). Slots see \`event_id\`
 * and \`value\`. Vocabulary: 0 = motion (0/1); detector blocks extend it. */
void zc_vision_emit(int event_id, int value)
{
    (void)event_id;
    (void)value;
${collectSlot(rendered, 'vision_event_cases', 4)}
}

esp_err_t app_vision_init(void)
{
    s_handle = app_driver_register_solution("vision", NULL, NULL);

    /* Camera + detector hardware — instance blocks, in product.yml order. */
${collectSlot(rendered, 'vision_node_init', 4)}

    ESP_LOGI(TAG, "Vision initialized");
    return ESP_OK;
}
`
}

/**
 * app_display.cpp — the LVGL touchscreen UI framework.
 *
 * Inverted binding like audio's: device types contribute a CARD (widgets +
 * event wiring) via display_widget_create, and param changes flow back into
 * widgets via display_driver_cb_cases.
 *
 * Lock discipline is centralized: the whole driver_cb case switch runs under
 * lvgl_port_lock (param-bus notifications arrive off the LVGL task). Widget
 * event callbacks run ON the LVGL task and must not lock. Slot code never
 * locks — a nested lock would deadlock.
 *
 * The panel is an instance block: its display_node_init creates the LVGL
 * display and assigns s_disp (framework blocks carry no cfg, so pins cannot
 * live here). A missing panel block is a loud init failure, not a blank
 * screen.
 */
function genAppDisplayCpp(rendered: RenderedInstance[]): string {
  const usedCard = new Set<string>()
  const cardFns: string[] = []
  const cardCalls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['display_widget_create'] ?? '').trim()) continue
    const slug = (r.prefix || r.block.id.split('/').pop() || 'card').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; usedCard.has(name); n++) name = `${slug}_${n}`
    usedCard.add(name)
    cardFns.push(`static void zc_${name}_display_card_init(lv_obj_t *parent)\n{\n${indentText(unwrapInitBody(r.slots['display_widget_create']), 4)}\n}`)
    cardCalls.push(`    zc_${name}_display_card_init(col);`)
  }
  return `#include "app_display.h"
#include "app_driver.h"
#include "app_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
// The emitted display_setup_task uses the FreeRTOS task API directly, so this
// file must include it itself. It used to arrive transitively via
// esp_lvgl_port.h, which is exactly the include-by-luck that broke seven
// sensor blocks when driver/i2c.h went away — a panel whose own includes do
// not pull FreeRTOS (the MIPI EK79007 does not) then fails on xTaskCreate.
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <esp_lvgl_port.h>
${collectSlot(rendered, 'display_includes')}

static const char *TAG = "app_display";

static app_driver_handle_t s_handle = 0;

/* Assigned by the panel driver block's display_node_init slot. */
static lv_display_t *s_disp = NULL;

${collectSlot(rendered, 'display_statics')}

/* Hardware/param bus -> widgets. Runs OFF the LVGL task, so the whole
 * dispatch holds the LVGL mutex (0 = wait forever). Slot cases must NOT
 * lock again. */
static void display_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
    (void)val;
    if (!lvgl_port_lock(0)) return;
    switch (param_id) {
${collectSlot(rendered, 'display_driver_cb_cases', 4)}
    default:
        break;
    }
    lvgl_port_unlock();
}

${cardFns.join('\n\n')}

/* Panel/touch bring-up + widget tree, in a task so it can NEVER block boot:
 * SPI panel init hangs forever on the emulator (and can stall on real
 * miswired hardware) — boot must still reach "ZeroCode AI firmware started"
 * or functional_testing times out on healthy firmware. Same degraded-state
 * philosophy as the camera driver. */
static void display_setup_task(void *arg)
{
    (void)arg;
    /* LVGL port init lives HERE too — under the emulator it never returns
     * (no SPI/LCD to drive), and it must not take app_main down with it. */
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    if (lvgl_port_init(&port_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init failed — display disabled");
        vTaskDelete(NULL);
        return;
    }

    /* Panel (and optional touch) hardware — instance blocks, in product.yml
     * order. The panel's slot assigns s_disp. */
${collectSlot(rendered, 'display_node_init', 4)}

    if (s_disp == NULL) {
        ESP_LOGE(TAG, "no display panel block in this product — add drivers/display_ili9341_spi");
        vTaskDelete(NULL);
        return;
    }

    /* One scrollable column of device cards, built under the LVGL lock. */
    if (lvgl_port_lock(0)) {
        lv_obj_t *col = lv_obj_create(lv_display_get_screen_active(s_disp));
        lv_obj_set_size(col, lv_pct(100), lv_pct(100));
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(col, 8, 0);
        /* This container is full-bleed, so EVERY touch lands on it or a card —
         * never on the screen. Without bubbling, a screen-level input handler
         * (the obvious place to hook backlight wake, screensaver dismiss, an
         * activity timer) is registered successfully and then never fires.
         * That is silent and survives review: on touch-room-panel-msmrkexj the
         * panel dimmed to 10% after 2 minutes and never woke, while both gates
         * asserted wake-on-touch worked. Bubble input events up so the screen
         * really does see them. */
        lv_obj_add_flag(col, LV_OBJ_FLAG_EVENT_BUBBLE);
${cardCalls.join('\n')}
        lvgl_port_unlock();
    }

    /* Register only once widgets exist — driver_cb cases null-check anyway. */
    s_handle = app_driver_register_solution("display", display_driver_cb, NULL);

    ESP_LOGI(TAG, "Display initialized");
    vTaskDelete(NULL);
}

esp_err_t app_display_init(void)
{
    xTaskCreate(display_setup_task, "display_setup", 6144, NULL, 4, NULL);
    ESP_LOGI(TAG, "Display setup started");
    return ESP_OK;
}
`
}

/**
 * app_mqtt.cpp — MQTT with Home Assistant discovery.
 *
 * Sibling of the other genApp*Cpp functions. The binding: each device-type
 * instance's mqtt_device_create publishes a retained HA discovery config,
 * subscribes its command topic, and publishes initial state; incoming
 * MQTT_EVENT_DATA dispatches through mqtt_command_cases (topic/payload in
 * scope); param-bus changes flow out through mqtt_driver_cb_cases.
 *
 * Wi-Fi lives HERE (like RainMaker's app_network) because a bare MQTT node
 * has no phone-app provisioning: creds + broker URI come from the serial
 * console (mqtt-wifi / mqtt-broker) and persist in NVS. An unconfigured
 * boot idles and logs — it must never block app_main.
 */
function genAppMqttCpp(rendered: RenderedInstance[]): string {
  // SHARED-NETWORK MODE: when a Wi-Fi-owning transport (Matter / RainMaker)
  // is co-selected, app_mqtt must NOT own Wi-Fi — no esp_wifi init, no
  // credentials, no mqtt-wifi console command. It waits for the transport's
  // network instead (IP_EVENT_STA_GOT_IP on the default event loop, plus an
  // already-has-IP check for the reboot-while-provisioned case) and starts
  // the MQTT client when connectivity appears. This is what makes
  // frameworks: [matter, mqtt] (a Matter device that also speaks Home
  // Assistant) composable.
  const sharedNet = rendered.some(r => isFramework(r, 'matter') || isFramework(r, 'rainmaker'))
  const usedDev = new Set<string>()
  const devFns: string[] = []
  const devCalls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['mqtt_device_create'] ?? '').trim()) continue
    const slug = (r.prefix || r.block.id.split('/').pop() || 'dev').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; usedDev.has(name); n++) name = `${slug}_${n}`
    usedDev.add(name)
    devFns.push(`static void zc_${name}_mqtt_device_init(void)\n{\n${indentText(unwrapInitBody(r.slots['mqtt_device_create']), 4)}\n}`)
    devCalls.push(`    zc_${name}_mqtt_device_init();`)
  }
  return `#include "app_mqtt.h"
#include "app_driver.h"
#include "app_config.h"
#include "app_console.h"

#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_console.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <nvs.h>
#include <mqtt_client.h>
${collectSlot(rendered, 'mqtt_includes')}

static const char *TAG = "app_mqtt";

static app_driver_handle_t s_handle = 0;
static esp_mqtt_client_handle_t s_client = NULL;
static volatile bool s_connected = false;
static bool s_wifi_started = false;

/* Node identity: station MAC as lowercase hex — stable, unique per board. */
static char s_node_id[13] = {0};
static char s_status_topic[48] = {0};

/* NVS-backed config (namespace "zc_mqtt"), set via the console commands. */
${sharedNet ? '' : `static char s_ssid[33] = {0};
static char s_pass[65] = {0};
`}static char s_uri[129] = {0};
static char s_user[33] = {0};
static char s_mqtt_pass[65] = {0};

${collectSlot(rendered, 'mqtt_statics')}

/* Connection-state transitions. Behavior blocks hook these via
 * mqtt_event_cases — \`connected\` is in scope (1 = broker session up). */
static void mqtt_on_event(int connected)
{
    (void)connected;
${collectSlot(rendered, 'mqtt_event_cases', 4)}
}

/* Hardware/param bus -> state topics. Publishes only while connected;
 * mqtt_device_create republishes current state on every (re)connect, so
 * nothing is lost while offline. */
static void mqtt_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
    (void)val;
    if (!s_connected || s_client == NULL) return;
    switch (param_id) {
${collectSlot(rendered, 'mqtt_driver_cb_cases', 4)}
    default:
        break;
    }
}

${devFns.join('\n\n')}

/* Command topics -> hardware. Runs on the MQTT task; payloads are not
 * NUL-terminated, so every case gets (topic, tlen, data, dlen). */
static void mqtt_handle_data(const char *topic, int tlen, const char *data, int dlen)
{
    (void)topic; (void)tlen; (void)data; (void)dlen;
${collectSlot(rendered, 'mqtt_command_cases', 4)}
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)arg; (void)base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            s_connected = true;
            ESP_LOGI(TAG, "MQTT connected");
            esp_mqtt_client_publish(s_client, s_status_topic, "online", 0, 1, true);
${devCalls.join('\n')}
            mqtt_on_event(1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_connected = false;
            ESP_LOGW(TAG, "MQTT disconnected — esp-mqtt reconnects on its own");
            mqtt_on_event(0);
            break;
        case MQTT_EVENT_DATA:
            mqtt_handle_data(event->topic, event->topic_len, event->data, event->data_len);
            break;
        default:
            break;
    }
}

static void mqtt_start_client(void)
{
    if (s_client != NULL || s_uri[0] == 0) return;
    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = s_uri;
    if (s_user[0]) cfg.credentials.username = s_user;
    if (s_mqtt_pass[0]) cfg.credentials.authentication.password = s_mqtt_pass;
    cfg.session.last_will.topic = s_status_topic;
    cfg.session.last_will.msg = "offline";
    cfg.session.last_will.qos = 1;
    cfg.session.last_will.retain = true;
    s_client = esp_mqtt_client_init(&cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "mqtt client init failed");
        return;
    }
    esp_mqtt_client_register_event(s_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    ESP_LOGI(TAG, "MQTT connecting to %s", s_uri);
}

${sharedNet ? `/* Shared-network mode: the transport (Matter/RainMaker) owns Wi-Fi. We only
 * listen for connectivity and start the client when it appears. BOTH v4 and
 * v6 events: Matter products often run IPv6-only (CONFIG_LWIP_IPV4=n), in
 * which case the broker URI must be a hostname or IPv6 literal. */
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)arg; (void)event_data; (void)base;
    if (event_id == IP_EVENT_STA_GOT_IP || event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "network up (transport-owned) — starting MQTT");
        mqtt_start_client();
    }
}` : `static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)arg; (void)event_data;
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected — retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi got IP");
        mqtt_start_client();
    }
}

static void mqtt_wifi_start(void)
{
    if (s_wifi_started || s_ssid[0] == 0) return;
    wifi_config_t wc = {};
    strlcpy((char *)wc.sta.ssid, s_ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, s_pass, sizeof(wc.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_started = true;
}`}

/* ── console config: the onboarding surface ───────────────────────── */

static void mqtt_load_config(void)
{
    nvs_handle_t h;
    if (nvs_open("zc_mqtt", NVS_READONLY, &h) != ESP_OK) return;
    size_t n;
${sharedNet ? '' : `    n = sizeof(s_ssid);      nvs_get_str(h, "ssid", s_ssid, &n);
    n = sizeof(s_pass);      nvs_get_str(h, "pass", s_pass, &n);
`}    n = sizeof(s_uri);       nvs_get_str(h, "uri", s_uri, &n);
    n = sizeof(s_user);      nvs_get_str(h, "user", s_user, &n);
    n = sizeof(s_mqtt_pass); nvs_get_str(h, "mpass", s_mqtt_pass, &n);
    nvs_close(h);
}

${sharedNet ? '' : `static int mqtt_wifi_cmd(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: mqtt-wifi <ssid> <password>\\n");
        return 1;
    }
    nvs_handle_t h;
    if (nvs_open("zc_mqtt", NVS_READWRITE, &h) != ESP_OK) return 1;
    nvs_set_str(h, "ssid", argv[1]);
    nvs_set_str(h, "pass", argv[2]);
    nvs_commit(h);
    nvs_close(h);
    strlcpy(s_ssid, argv[1], sizeof(s_ssid));
    strlcpy(s_pass, argv[2], sizeof(s_pass));
    printf("wifi config saved — connecting\\n");
    if (s_wifi_started) {
        wifi_config_t wc = {};
        strlcpy((char *)wc.sta.ssid, s_ssid, sizeof(wc.sta.ssid));
        strlcpy((char *)wc.sta.password, s_pass, sizeof(wc.sta.password));
        esp_wifi_set_config(WIFI_IF_STA, &wc);
        esp_wifi_disconnect();
        esp_wifi_connect();
    } else {
        mqtt_wifi_start();
    }
    return 0;
}
`}static int mqtt_broker_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: mqtt-broker <uri> [username] [password]  (e.g. mqtt://192.168.1.10)\\n");
        return 1;
    }
    nvs_handle_t h;
    if (nvs_open("zc_mqtt", NVS_READWRITE, &h) != ESP_OK) return 1;
    nvs_set_str(h, "uri", argv[1]);
    nvs_set_str(h, "user", argc > 2 ? argv[2] : "");
    nvs_set_str(h, "mpass", argc > 3 ? argv[3] : "");
    nvs_commit(h);
    nvs_close(h);
    strlcpy(s_uri, argv[1], sizeof(s_uri));
    strlcpy(s_user, argc > 2 ? argv[2] : "", sizeof(s_user));
    strlcpy(s_mqtt_pass, argc > 3 ? argv[3] : "", sizeof(s_mqtt_pass));
    printf("broker config saved\\n");
    if (s_client != NULL) {
        /* URI changed under a live client: restart it clean. */
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        s_connected = false;
    }
    mqtt_start_client();
    return 0;
}

esp_err_t app_mqtt_init(void)
{
    s_handle = app_driver_register_solution("mqtt", mqtt_driver_cb, NULL);

${sharedNet ? `    /* Transport-owned network: create-if-missing is fine either way (the
     * transport usually created the default loop before us; render order is
     * the product author's). */
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(loop_err);
    }
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, wifi_event_handler, NULL));` : `    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));`}

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_node_id, sizeof(s_node_id), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(s_status_topic, sizeof(s_status_topic), "zerocode/%s/status", s_node_id);

    /* Node-level contributions (behaviors) — before anything connects. */
${collectSlot(rendered, 'mqtt_node_init', 4)}

${sharedNet ? '' : `    static const esp_console_cmd_t wifi_cmd = {
        .command = "mqtt-wifi",
        .help = "Set Wi-Fi credentials: mqtt-wifi <ssid> <password>",
        .hint = NULL,
        .func = &mqtt_wifi_cmd,
        .argtable = NULL,
    };
    app_console_register_cmd(&wifi_cmd);
`}    static const esp_console_cmd_t broker_cmd = {
        .command = "mqtt-broker",
        .help = "Set broker: mqtt-broker <uri> [username] [password]",
        .hint = NULL,
        .func = &mqtt_broker_cmd,
        .argtable = NULL,
    };
    app_console_register_cmd(&broker_cmd);

    mqtt_load_config();
${sharedNet ? `    if (s_uri[0] == 0) {
        /* Unconfigured is a valid state — never block boot. Wi-Fi comes from
         * the transport's own onboarding (commissioning / provisioning). */
        ESP_LOGW(TAG, "MQTT broker not configured — run: mqtt-broker <uri>");
    }
    /* Reboot-while-provisioned: the transport may already have an IP before
     * we registered the handler. The v4 check is compiled out on IPv6-only
     * Matter builds (CONFIG_LWIP_IPV4=n removes esp_netif_get_ip_info). */
#if CONFIG_LWIP_IPV4
    {
        esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_ip_info_t ip = {};
        if (sta != NULL && esp_netif_get_ip_info(sta, &ip) == ESP_OK && ip.ip.addr != 0) {
            ESP_LOGI(TAG, "network already up — starting MQTT");
            mqtt_start_client();
        }
    }
#endif` : `    if (s_ssid[0] == 0 || s_uri[0] == 0) {
        /* Unconfigured is a valid state — never block boot. */
        ESP_LOGW(TAG, "MQTT not configured — run: mqtt-wifi <ssid> <pass>, then mqtt-broker <uri>");
    }
    mqtt_wifi_start();`}

    ESP_LOGI(TAG, "MQTT initialized (node %s)", s_node_id);
    return ESP_OK;
}
`
}

/**
 * app_audio.cpp — the esp-sr voice framework.
 *
 * Structurally the sibling of genAppMatterCpp/genAppRainmakerCpp/genAppZigbeeCpp,
 * but the binding is inverted: device types contribute PHRASES rather than
 * endpoints, and a recognized phrase id drives app_driver_set_param.
 *
 * Two tasks by necessity, not preference: AFE's feed and fetch halves must run
 * concurrently (feed blocks on I2S, fetch blocks on AFE's internal queue), so
 * running them on one task deadlocks as soon as either side stalls.
 */
function genAppAudioCpp(rendered: RenderedInstance[]): string {
  return `#include "app_audio.h"
#include "app_driver.h"
#include "app_config.h"

#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_afe_config.h>
#include <esp_afe_sr_iface.h>
#include <esp_afe_sr_models.h>
#include <esp_mn_iface.h>
#include <esp_mn_models.h>
#include <esp_mn_speech_commands.h>
#include <model_path.h>
${collectSlot(rendered, 'audio_includes')}

static const char *TAG = "app_audio";

static app_driver_handle_t s_handle = 0;

static srmodel_list_t *s_models = NULL;
static const esp_afe_sr_iface_t *s_afe = NULL;
static esp_afe_sr_data_t *s_afe_data = NULL;
static const esp_mn_iface_t *s_mn = NULL;
static model_iface_data_t *s_mn_data = NULL;

${collectSlot(rendered, 'audio_statics')}

/* Recognized phrase -> hardware. Device types own the cases. */
static void audio_apply_command(int phrase_id)
{
    switch (phrase_id) {
${collectSlot(rendered, 'audio_command_cases', 4)}
    default:
        break;
    }
    /* Reached only when no case claimed the phrase — every device-type case
     * returns. A phrase registered with no matching case means the block's
     * audio_commands_register and audio_command_cases disagree. */
    ESP_LOGW(TAG, "phrase %d recognized but no device type claims it", phrase_id);
}

/* Listening-state transitions. Behavior blocks hook these via
 * audio_event_cases — \`listening\` is in scope: 1 = wake word detected,
 * command window open; 0 = window closed (command handled or timed out).
 * The audio analog of matter_event_cases / zigbee_signal_cases. */
static void audio_on_event(int listening)
{
    (void)listening;
${collectSlot(rendered, 'audio_event_cases', 4)}
}

/* Hardware/param bus -> voice feedback. Device types may announce state here. */
static void audio_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)param_id;
    (void)val;
    (void)source;
    (void)ctx;
${collectSlot(rendered, 'audio_driver_cb_cases', 4)}
}

static void audio_feed_task(void *arg)
{
    (void)arg;
    const int chunk = s_afe->get_feed_chunksize(s_afe_data);
    const int channels = s_afe->get_feed_channel_num(s_afe_data);
    int16_t *buf = (int16_t *)heap_caps_malloc(chunk * channels * sizeof(int16_t),
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE(TAG, "feed buffer alloc failed (%d samples)", chunk * channels);
        vTaskDelete(NULL);
        return;
    }
    while (1) {
        /* Provided by drivers/voice_mic_i2s (fixed-name contract, app_audio.h). */
        if (zc_audio_mic_read(buf, chunk * channels) == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        s_afe->feed(s_afe_data, buf);
    }
}

static void audio_detect_task(void *arg)
{
    (void)arg;
    /* Wake word gates command recognition: MultiNet only runs in the window
     * after a detected wake word, which is what keeps idle CPU down. */
    bool listening = false;

    while (1) {
        afe_fetch_result_t *res = s_afe->fetch(s_afe_data);
        if (!res || res->ret_value == ESP_FAIL) continue;

        if (!listening && res->wakeup_state == WAKENET_DETECTED) {
            listening = true;
            s_mn->clean(s_mn_data);
            ESP_LOGI(TAG, "wake word detected — listening for a command");
            audio_on_event(1);
            continue;
        }

        if (!listening) continue;

        esp_mn_state_t st = s_mn->detect(s_mn_data, res->data);
        if (st == ESP_MN_STATE_DETECTING) continue;

        if (st == ESP_MN_STATE_DETECTED) {
            esp_mn_results_t *mn = s_mn->get_results(s_mn_data);
            if (mn && mn->num > 0) {
                ESP_LOGI(TAG, "command phrase %d", mn->phrase_id[0]);
                audio_apply_command(mn->phrase_id[0]);
            }
        } else if (st == ESP_MN_STATE_TIMEOUT) {
            ESP_LOGI(TAG, "no command heard — sleeping until next wake word");
        }
        listening = false;
        audio_on_event(0);
    }
}

esp_err_t app_audio_init(void)
{
    s_handle = app_driver_register_solution("audio", audio_driver_cb, NULL);

    /* Capture hardware first — AFE has nothing to read without it. */
${collectSlot(rendered, 'audio_node_init', 4)}

    /* Models live in the \`model\` partition; see the 8mb-voice table. A build
     * with no model partition reaches here and fails, which is the intended
     * loud failure rather than a silent mis-detection. */
    s_models = esp_srmodel_init("model");
    if (!s_models || s_models->num <= 0) {
        ESP_LOGE(TAG, "no esp-sr models found — is the 'model' partition flashed?");
        return ESP_ERR_NOT_FOUND;
    }

    char *wn_name = esp_srmodel_filter(s_models, ESP_WN_PREFIX, NULL);
    char *mn_name = esp_srmodel_filter(s_models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    if (!wn_name || !mn_name) {
        ESP_LOGE(TAG, "missing model (wakenet=%s multinet=%s)",
                 wn_name ? wn_name : "none", mn_name ? mn_name : "none");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "models: wakenet=%s multinet=%s", wn_name, mn_name);

    /* "M" = one mic channel, no reference channel (no AEC — nothing plays back
     * into this mic in the reference products). */
    afe_config_t *afe_cfg = afe_config_init("M", s_models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    if (!afe_cfg) {
        ESP_LOGE(TAG, "afe_config_init failed");
        return ESP_ERR_NO_MEM;
    }
    s_afe = esp_afe_handle_from_config(afe_cfg);
    s_afe_data = s_afe->create_from_config(afe_cfg);
    afe_config_free(afe_cfg);
    if (!s_afe_data) {
        ESP_LOGE(TAG, "AFE create failed");
        return ESP_ERR_NO_MEM;
    }

    /* 6000 ms command window after the wake word. */
    s_mn = esp_mn_handle_from_name(mn_name);
    s_mn_data = s_mn->create(mn_name, 6000);
    if (!s_mn_data) {
        ESP_LOGE(TAG, "MultiNet create failed");
        return ESP_ERR_NO_MEM;
    }

    /* Phrases are registered at runtime, not from sdkconfig: the vocabulary is
     * composed from whichever device types the product includes. */
    esp_mn_commands_clear();
${collectSlot(rendered, 'audio_commands_register', 4)}
    esp_mn_commands_update();
    s_mn->print_active_speech_commands(s_mn_data);

    xTaskCreate(audio_feed_task, "audio_feed", 4096, NULL, 5, NULL);
    xTaskCreate(audio_detect_task, "audio_detect", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Voice framework initialized");
    return ESP_OK;
}
`
}

function dedent(s: string): string {
  const lines = s.split('\n')
  const indents = lines.filter(l => l.trim()).map(l => l.match(/^ */)![0].length)
  const min = indents.length ? Math.min(...indents) : 0
  return lines.map(l => l.slice(min)).join('\n')
}

function indentText(s: string, n: number): string {
  const pad = ' '.repeat(n)
  return s.split('\n').map(l => (l.length ? pad + l : l)).join('\n')
}

/** Strip the outer `{ }` scope block a behavior's logic_init was authored with.
 *  That block only existed to isolate locals when every behavior's init was
 *  inlined into one app_logic_init(); as its own function it's redundant (and
 *  the bare blocks confuse editing). */
function unwrapInitBody(slot: string): string {
  let s = slot.replace(/^\n+|\n+$/g, '')
  const t = s.trim()
  if (t.startsWith('{') && t.endsWith('}')) {
    const open = s.indexOf('{')
    const close = s.lastIndexOf('}')
    s = dedent(s.slice(open + 1, close)).replace(/^\n+|\n+$/g, '')
  }
  return s
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
    s_button_count++;
    ESP_LOGI(TAG, "created shared button device on GPIO %d", gpio_num);
    return handle;
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
    if (!(r.slots['logic_statics'] ?? '').trim() && !(r.slots['logic_init'] ?? '').trim()) continue
    const cat = logicCategory(r.block)
    if (!groups.has(cat)) groups.set(cat, [])
    groups.get(cat)!.push(r)
  }
  const parts: Array<{ name: string; content: string }> = []
  const calls: string[] = []
  const decls: string[] = []
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
    const used = new Set<string>()
    let usesTag = false
    const bodyLines: string[] = []
    for (const r of members) {
      const statics = (r.slots['logic_statics'] ?? '').trim()
      const initRaw = (r.slots['logic_init'] ?? '').trim()
      usesTag = usesTag || /\bTAG\b/.test(`${statics} ${initRaw}`)
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
    }
    if (usesTag) lines.push(`static const char *TAG = "${cat}";`, '')
    lines.push(...bodyLines)
    lines.push(`void ${cat}_logic_init(void)`, '{', ...innerCalls, '}', '')
    parts.push({ name: `${cat}.cpp`, content: lines.join('\n') })
    calls.push(`    ${cat}_logic_init();`)
    decls.push(`void ${cat}_logic_init(void);`)
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
      /(PRIV_REQUIRES\s+)([^\n)]*)/,
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
 *  0.7.1 is the version published to the component registry and the version
 *  of the pinned esp-board-manager checkout that supplies the `bmgr` action
 *  (the platform's pinned esp-board-manager checkout); `~` keeps the two on one minor. */
const BOARD_MANAGER_VERSION = '~0.7.1'

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
}

/** What the engine reads out of a board pack at GENERATION time. */
interface BoardFacts {
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
async function loadBoardFacts(paths: GeneratorPaths, board: string): Promise<BoardFacts | null> {
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
        devices.push({ name: e.name, type: e.type })
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

/** The LVGL binding for a board-provided panel, as `display_node_init` code.
 *  `s_disp`, `TAG` and the LVGL port are all in scope — this renders inside
 *  app_display.cpp's display_setup_task, after `lvgl_port_init()`. */
function boardDisplayNodeInit(board: string, lcd: BoardDevice, touch: BoardDevice | null): string {
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
        board_disp_cfg.flags.swap_bytes = board_lcd_serial ? 1 : 0;
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
    slots.display_node_init = boardDisplayNodeInit(board, lcd, touch)
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

    for (const guard of map.requires ?? []) {
      if (!guard || typeof guard.cfg !== 'string' || guard.reason === undefined) {
        throw new Error(`block ${r.block.id}: each bmgr.requires entry needs 'cfg', 'equals' and 'reason'`)
      }
      if (!(guard.cfg in (r.block.params ?? {}))) {
        // A guard on a param that does not exist always "passes" by accident.
        throw new Error(`block ${r.block.id}: bmgr.requires guards cfg.${guard.cfg}, which the block does not declare`)
      }
      const actual = r.cfg[guard.cfg]
      if (String(actual) !== String(guard.equals)) {
        throw new Error(
          `product '${product.id}': ${where} cannot be handed to the board manager — ` +
          `cfg.${guard.cfg} is ${JSON.stringify(actual)}, and the block's bmgr mapping is only ` +
          `declared valid for ${JSON.stringify(guard.equals)}.\n` +
          `  Reason: ${guard.reason}\n` +
          `  Fix: use a configuration the block vouches for, or clear bmgr.board ('${bmgrBoard}') ` +
          `in this instance's board.yaml, so the block keeps its own initialization.`,
        )
      }
    }

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
