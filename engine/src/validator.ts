// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Static validation for the block-composition system.
 *
 * Pure functions over already-loaded Block + Product objects. Two consumers:
 *   - scripts/validate-templates.ts walks the on-disk catalog and runs both
 *     validateBlock + validateProduct on everything.
 *   - applyTemplate (in index.ts) runs validateProduct against a single
 *     product before generation, so the agent gets a structured error
 *     before the generator writes a partial firmware tree.
 *
 * Returns issues instead of throwing so callers can decide how to surface
 * them (CLI prints them; service throws on errors).
 */

import * as fs from 'node:fs'
import * as os from 'node:os'
import * as path from 'node:path'
import { parse as parseYaml } from 'yaml'
import { chipCapabilities } from './hardware.js'
import {
  type Block,
  type BoardFile,
  type BusDecl,
  type Product,
  VALUE_TYPES,
  type ValueType,
  bmgrBoardOf,
  boardTakesOver,
} from './types.js'

export interface ValidationIssue {
  level: 'error' | 'warning'
  product?: string
  block?: string
  message: string
}

export interface ValidationResult {
  errors: ValidationIssue[]
  warnings: ValidationIssue[]
}

export function mergeResults(...rs: ValidationResult[]): ValidationResult {
  return {
    errors: rs.flatMap(r => r.errors),
    warnings: rs.flatMap(r => r.warnings),
  }
}

const VALID_BLOCK_KINDS = [
  'peripheral',
  'driver',
  'device_type',
  'binding',
  'behavior',
  'framework',
  'partition_table',
  'sdkconfig_fragment',
]

export function validateBlock(block: Block): ValidationResult {
  const errors: ValidationIssue[] = []
  const warnings: ValidationIssue[] = []
  const ctx = { block: block.id }

  if (!block.kind) {
    warnings.push({ level: 'warning', ...ctx, message: 'block has no kind' })
  } else if (!VALID_BLOCK_KINDS.includes(block.kind)) {
    errors.push({ level: 'error', ...ctx, message: `unknown kind '${block.kind}'` })
  }

  for (const [refKey, refType] of Object.entries(block.param_refs ?? {})) {
    if (!VALUE_TYPES.includes(refType as ValueType)) {
      errors.push({
        level: 'error',
        ...ctx,
        message: `param_refs.${refKey} has invalid type '${refType}' (expected ${VALUE_TYPES.join('|')})`,
      })
    }
    if (!block.params?.[refKey]) {
      errors.push({
        level: 'error',
        ...ctx,
        message: `param_refs.${refKey} references undeclared cfg param '${refKey}'`,
      })
    }
  }

  // provides_bus / requires_bus: a declaration naming a param the block does
  // not have is worse than none — it resolves to an empty port and quietly
  // matches (or fails to match) the wrong thing. See BusDecl.
  for (const key of ['provides_bus', 'requires_bus'] as const) {
    for (const decl of busDecls(block[key])) {
      if (!decl.type || typeof decl.type !== 'string') {
        errors.push({ level: 'error', ...ctx, message: `${key} entry needs a 'type' (i2c, spi, …)` })
      }
      if (!decl.port_param || typeof decl.port_param !== 'string') {
        errors.push({ level: 'error', ...ctx, message: `${key} entry needs a 'port_param' naming the cfg key that holds the bus index` })
      } else if (!block.params?.[decl.port_param]) {
        errors.push({
          level: 'error',
          ...ctx,
          message: `${key} names port_param '${decl.port_param}', which the block does not declare in params`,
        })
      }
    }
  }

  const declared = new Set(Object.keys(block.params ?? {}))
  for (const [slotName, body] of Object.entries(block.slots ?? {})) {
    const re = /\{\{\s*cfg\.([A-Za-z0-9_]+)/g
    let m: RegExpExecArray | null
    while ((m = re.exec(body)) !== null) {
      if (!declared.has(m[1])) {
        errors.push({
          level: 'error',
          ...ctx,
          message: `slot '${slotName}' references {{cfg.${m[1]}}} but params has no '${m[1]}'`,
        })
      }
    }
  }

  return { errors, warnings }
}

/** `provides_bus:` / `requires_bus:` as a list, whichever way it was written.
 *  A single mapping is the common case and reads better in a block.yml; a list
 *  is what a block needing two buses (an SPI panel with an I2C touch) needs. */
export function busDecls(node: BusDecl | BusDecl[] | undefined): BusDecl[] {
  if (node === undefined || node === null) return []
  return (Array.isArray(node) ? node : [node]).filter(d => d && typeof d === 'object')
}

/** The bus index an instance is on: its cfg value, else the block default.
 *  Compared as a STRING — `0`, `"0"` and `I2C_NUM_0` are all somebody's way of
 *  writing a port, and only equality matters here. */
function busPort(block: Block, cfg: Record<string, unknown>, decl: BusDecl): string {
  const raw = cfg[decl.port_param] ?? block.params?.[decl.port_param]?.default
  return raw === undefined || raw === null ? '' : String(raw)
}

/** Does the BOARD itself already configure this bus? A `board:` product's
 *  on-board hardware comes from the board definition, so an `i2c` peripheral
 *  in board_peripherals.yaml on the right port is a provider exactly as an
 *  instance of peripherals/i2c_bus would be — and requiring the instance too
 *  would mean configuring one bus twice. */
export function boardProvidesBus(boardDir: string, type: string, port: string): string | null {
  const doc = readYaml(path.join(boardDir, 'board_peripherals.yaml')) as { peripherals?: unknown } | undefined
  for (const p of (Array.isArray(doc?.peripherals) ? doc.peripherals : []) as Record<string, unknown>[]) {
    if (p?.type !== type) continue
    const cfgPort = (p?.config as Record<string, unknown> | undefined)?.port
    if (cfgPort === undefined || String(cfgPort) !== port) continue
    return typeof p?.name === 'string' ? p.name : `(unnamed ${type})`
  }
  return null
}

interface ParamRef {
  name: string
  type: ValueType
  emittedBy: string[]
  consumedBy: string[]
}

/* ── esp-board-manager board packs ────────────────────────────────────────
 *
 * A board file's `bmgr.board` names an esp-board-manager board definition,
 * which is HOST CONTENT — like ESP-IDF itself, it lives outside this repo and
 * is not guaranteed to be on the machine running validation. Layout:
 *
 *   <root>/<pack>/<board>/board_info.yaml        (a directory of packs)
 *   <root>/<board>/board_info.yaml               (a bare single pack)
 *
 * When <root> does not exist at all we SKIP the board checks with a warning
 * rather than failing, so `node scripts/validate.mjs` stays runnable with no
 * board packs checked out. */

export interface BoardCheckOptions {
  /** Root directory holding board packs. Defaults to $ZC_BOARDS_DIR — point
   *  it at an esp-board-manager checkout's `boards/` (or a directory of such
   *  packs). Unset, the board checks are skipped with a warning. */
  boardsDir?: string
  /** The hardware this product instance runs on — a parsed board.yaml. The
   *  board checks below run only when one is supplied; a catalog product on
   *  its own is GENERAL, so there is no board to check it against and nothing
   *  here fires. Not discovered from the product: the caller owns the answer
   *  (a host reads the user's, `scripts/validate.mjs` reads the two samples'). */
  board?: BoardFile | null
}

/** Where the packs are looked for when nothing names them: a directory that
 *  exists on no machine by accident, so an unset ZC_BOARDS_DIR reads as "no
 *  packs here" rather than as somebody's checkout. */
export const DEFAULT_BOARDS_DIR = path.join(os.homedir(), '.zerocode', 'boards')

export function boardsRoot(opts: BoardCheckOptions = {}): string {
  return opts.boardsDir ?? process.env.ZC_BOARDS_DIR ?? DEFAULT_BOARDS_DIR
}

/** Absolute path of <board>'s directory inside the packs under `root`, or null. */
export function findBoardDir(root: string, board: string): string | null {
  const direct = path.join(root, board, 'board_info.yaml')
  if (fs.existsSync(direct)) return path.join(root, board)
  let packs: string[]
  try {
    // statSync, not the Dirent's isDirectory(): a pack root is commonly a
    // directory of SYMLINKS (the ZeroCode image and its local build script
    // assemble upstream's packs and the staging leftovers that way), and a
    // Dirent reports a symlink as not-a-directory — which made every board
    // "not found" while `ls <root>/<pack>/<board>` showed it plainly.
    packs = fs.readdirSync(root).filter((name) => {
      try { return fs.statSync(path.join(root, name)).isDirectory() } catch { return false }
    })
  } catch {
    return null
  }
  for (const pack of packs) {
    const cand = path.join(root, pack, board)
    if (fs.existsSync(path.join(cand, 'board_info.yaml'))) return cand
  }
  return null
}

function readYaml(file: string): unknown {
  try {
    return parseYaml(fs.readFileSync(file, 'utf-8'))
  } catch {
    return undefined
  }
}

/** `chip:` from a board's board_info.yaml, or undefined if unreadable. */
export function boardChip(boardDir: string): string | undefined {
  const info = readYaml(path.join(boardDir, 'board_info.yaml')) as { chip?: unknown } | undefined
  return typeof info?.chip === 'string' ? info.chip : undefined
}

/* Which YAML keys hold a GPIO number. bmgr's shapes vary by peripheral and
 * device type, so this is a shape rule rather than a fixed list:
 *   pin, *_pin            gpio/button peripherals, camera xclk_pin
 *   gpio_num, *_gpio_num  cs/dc/reset/int/wr/strip_gpio_num
 *   gpio_*                knob gpio_encoder_a/b
 *   *_io_num, *_io_<n>    spi sclk/mosi/miso/quadwp/quadhd, uart tx/rx/rts/cts,
 *                         sdmmc data_io_0..7
 * plus every DIRECT integer under a `pins:` map (sda/scl/mclk/bclk/ws/dout/
 * din/clk/cmd/d0..d7) — direct only, so `pins.invert_flags` is not descended.
 * Keys like `port`, `spi_port`, `uart_num`, `channel_id`, `bus_id` are
 * deliberately excluded: they are bus indices, not pins. */
function isPinKey(key: string): boolean {
  return (
    key === 'pin' || key.endsWith('_pin') ||
    key === 'gpio_num' || key.endsWith('_gpio_num') ||
    key.endsWith('_io_num') || /_io_\d+$/.test(key) ||
    // gpio_encoder_a / gpio_encoder_b: the knob device (bmgr v0.7.2) keeps
    // its pins under a gpio_ PREFIX. Without this the M5Dial's encoder pins
    // validated as free.
    key.startsWith('gpio_')
  )
}

function collectPins(node: unknown, label: string, out: Map<number, string>, keyPath = ''): void {
  if (Array.isArray(node)) {
    for (const item of node) collectPins(item, label, out, keyPath)
    return
  }
  if (node === null || typeof node !== 'object') return
  for (const [key, value] of Object.entries(node as Record<string, unknown>)) {
    const here = keyPath ? `${keyPath}.${key}` : key
    if (key === 'pins' && value !== null && typeof value === 'object' && !Array.isArray(value)) {
      // Direct integer children only — a nested map here (invert_flags) is flags.
      for (const [pk, pv] of Object.entries(value as Record<string, unknown>)) {
        if (typeof pv === 'number' && Number.isInteger(pv) && pv >= 0) {
          if (!out.has(pv)) out.set(pv, `${label} ${here}.${pk}`)
        }
      }
      continue
    }
    if (typeof value === 'number') {
      // -1 is bmgr's "line not present" (e.g. a QSPI panel's dc_gpio_num).
      if (isPinKey(key) && Number.isInteger(value) && value >= 0 && !out.has(value)) {
        out.set(value, `${label} ${here}`)
      }
      continue
    }
    collectPins(value, label, out, here)
  }
}

/** The devices a board definition declares, as name + type. The pair a
 *  board-device adapter is matched against: TYPE says which handle struct
 *  comes back, NAME is what `esp_board_manager_get_device_handle` is called
 *  with, and a board may carry several devices of one type. */
export function boardDeclaredDevices(boardDir: string): Array<{ name: string; type: string }> {
  const doc = readYaml(path.join(boardDir, 'board_devices.yaml')) as
    { devices?: unknown } | undefined
  const out: Array<{ name: string; type: string }> = []
  for (const d of (Array.isArray(doc?.devices) ? doc.devices : []) as Record<string, unknown>[]) {
    if (typeof d?.name === 'string' && typeof d?.type === 'string') out.push({ name: d.name, type: d.type })
  }
  return out
}

/** boardPinOwners() flattened to pin → description. Kept for callers that
 *  only need "is this pin taken, and by what" without the device attribution. */
export function boardOccupiedPins(boardDir: string): Map<number, string> {
  return new Map([...boardPinOwners(boardDir)].map(([pin, o]) => [pin, o.desc]))
}

/** Who claims a board pin. `device` is set when the claim is attributable to a
 *  board DEVICE — either the device's own config names the pin (a led_strip's
 *  strip_gpio_num) or the device references the claiming peripheral by name
 *  (boot_button → gpio_boot_button). That attribution is what lets the
 *  pin-conflict check tell "collides with the board" apart from "the product
 *  deliberately drives this board device through an adapter". */
export interface BoardPinOwner {
  desc: string
  device?: { name: string; type: string }
}

/** Every GPIO the board's own peripherals/devices already claim, mapped to
 *  who claims it. Read from the board YAML directly: bmgr's
 *  `gen_board_metadata.yaml` has the same information in an `io:` section,
 *  but it only exists AFTER generation, which is too late here.
 *
 *  DEVICES are walked first, each pulling in the peripherals it references
 *  (`peripherals: [- gpio_name: …]` — the values are peripheral names), so a
 *  pin owned by a device-backed peripheral is attributed to the device.
 *  Standalone peripherals follow. First claim wins, as before.
 *
 *  Under-reports on purpose in two places, both false-NEGATIVE (a conflict we
 *  miss, never one we invent — the safe direction for an error-level check):
 *    - ADC peripherals name a `channel_id` on a `unit_id`, not a GPIO. The
 *      channel→pin map is per-chip and lives nowhere in the board YAML, so an
 *      ADC-occupied pin is invisible here. Treating channel_id as a pin would
 *      be strictly worse: it would flag an unrelated GPIO.
 *    - The "Not modeled:" prose blocks (module slots, buttons on a
 *      power-management path, unmodelled I2C devices) name real pins in
 *      comments that no parser sees. */
export function boardPinOwners(boardDir: string): Map<number, BoardPinOwner> {
  const out = new Map<number, BoardPinOwner>()
  const periphDoc = readYaml(path.join(boardDir, 'board_peripherals.yaml')) as
    { peripherals?: unknown } | undefined
  const periphs = (Array.isArray(periphDoc?.peripherals) ? periphDoc.peripherals : []) as Record<string, unknown>[]
  const periphByName = new Map(periphs.filter(p => typeof p?.name === 'string').map(p => [p.name as string, p]))

  const put = (pins: Map<number, string>, device?: { name: string; type: string }) => {
    for (const [pin, desc] of pins) if (!out.has(pin)) out.set(pin, { desc, device })
  }

  const devDoc = readYaml(path.join(boardDir, 'board_devices.yaml')) as
    { devices?: unknown } | undefined
  for (const d of (Array.isArray(devDoc?.devices) ? devDoc.devices : []) as Record<string, unknown>[]) {
    const name = typeof d?.name === 'string' ? d.name : '(unnamed)'
    const type = typeof d?.type === 'string' ? d.type : ''
    const pins = new Map<number, string>()
    collectPins(d, `device '${name}'${type ? ` ${type}` : ''}`, pins)
    for (const ref of (Array.isArray(d?.peripherals) ? d.peripherals : []) as Record<string, unknown>[]) {
      if (ref === null || typeof ref !== 'object') continue
      for (const v of Object.values(ref)) {
        const p = typeof v === 'string' ? periphByName.get(v) : undefined
        if (p) collectPins(p, `peripheral '${String(v)}' (device '${name}'${type ? ` ${type}` : ''})`, pins)
      }
    }
    put(pins, type ? { name, type } : undefined)
  }
  for (const p of periphs) {
    const name = typeof p?.name === 'string' ? p.name : '(unnamed)'
    const type = typeof p?.type === 'string' ? ` ${p.type}` : ''
    const pins = new Map<number, string>()
    collectPins(p, `peripheral '${name}'${type}`, pins)
    put(pins)
  }
  return out
}

export function validateProduct(
  product: Product,
  blocks: Map<string, Block>,
  boardOpts: BoardCheckOptions = {},
): ValidationResult {
  const errors: ValidationIssue[] = []
  const warnings: ValidationIssue[] = []
  const ctx = { product: product.id }

  if (!product.id) {
    errors.push({ level: 'error', message: 'product is missing required `id:` field' })
  }
  if (!product.instances || product.instances.length === 0) {
    errors.push({ level: 'error', ...ctx, message: 'product has no instances' })
    return { errors, warnings }
  }

  // FLASH-BUDGET GATES (audio) — same rules the generator enforces, surfaced
  // at catalog-validation time so a bad product never reaches a build.
  const frameworks = product.frameworks ?? []
  if (frameworks.includes('audio')) {
    const table = product.partition_table ?? ''
    if (!['8mb-voice', '16mb-voice'].includes(table)) {
      errors.push({
        level: 'error',
        ...ctx,
        message: `audio framework needs a 'model' partition — set partition_table: 8mb-voice (or 16mb-voice with a Wi-Fi transport); got '${table || '(default)'}'`,
      })
    }
    const bigTransports = ['matter', 'rainmaker', 'ble_mesh'].filter((f) => frameworks.includes(f))
    if (bigTransports.length > 0 && table !== '16mb-voice') {
      errors.push({
        level: 'error',
        ...ctx,
        message: `audio + ${bigTransports.join('+')} needs 16MB flash (98% OTA-slot fill measured on 8mb-voice) — set partition_table: 16mb-voice`,
      })
    }
  }

  // FLASH-BUDGET GATE (lm). The story model is 2.0 MB of weights EMBEDded in
  // the app image, and 2.0 MB + ~255 KB of app needs 2,349,504 B against the
  // default 4mb table's 1,966,080 B OTA slot — 20% short. Unchecked, that is
  // an image that builds and then overflows at flash time, which is the same
  // class of failure the audio gate above exists to prevent.
  if (frameworks.includes('lm')) {
    const table = product.partition_table ?? ''
    const wantsStory = (product.instances ?? []).some(
      (i) => i.block === 'behaviors/lm_storyteller',
    )
    if (wantsStory && !['8mb', '8mb-voice', '16mb-voice'].includes(table)) {
      errors.push({
        level: 'error',
        ...ctx,
        message:
          `behaviors/lm_storyteller embeds a 2.0 MB model — 2,349,504 B with the app, which does not ` +
          `fit the ${table || '4mb (default)'} table's OTA slot (the default 4mb slot is 1,966,080 B). ` +
          `Set partition_table: 8mb (3 MB slot, 75% full).`,
      })
    }
  }

  // MEMORY GATES (ml). The tensor arena is the one number a product cannot
  // guess and cannot discover without running the model, and getting it wrong
  // fails at RUNTIME (AllocateTensors returns error, the block logs "inference
  // disabled" and the device silently does nothing) — not at build time. That
  // is the worst failure shape available, so it is checked here.
  if (frameworks.includes('ml')) {
    const chips = (product as { ci_chips?: string[] }).ci_chips ?? []
    // Chips whose esp-nn kernels are compiled as optimized assembly, so
    // CONFIG_NN_OPTIMIZED is on and the model's scratch buffer is allocated.
    // Everything else takes the generic-C path and needs only the base.
    const NN_OPTIMIZED_CHIPS = ['esp32s3', 'esp32p4', 'esp32s31']
    // Parts with PSRAM available at all. arena_in_psram on anything else is
    // not an error — heap_caps_malloc_prefer falls back — but the fallback is
    // silent and spends that many KB of INTERNAL RAM instead, which is the
    // opposite of what the author asked for.
    const PSRAM_CHIPS = ['esp32', 'esp32s3', 'esp32p4', 'esp32s31']

    for (const inst of product.instances ?? []) {
      const cfg = (inst.cfg ?? {}) as Record<string, unknown>
      // cfg value, else the block-param default — the busPort idiom. Reading
      // raw cfg alone let a product that overrides arena_base_kb but omits
      // arena_scratch_kb (inheriting the classifier's default 0) dodge the
      // exact warning built for that shape.
      const block = blocks.get(inst.block)
      const cfgOr = (key: string): unknown => cfg[key] ?? block?.params?.[key]?.default
      const base = Number(cfgOr('arena_base_kb') ?? NaN)
      const scratch = Number(cfgOr('arena_scratch_kb') ?? NaN)

      if (Number.isFinite(base) && Number.isFinite(scratch)) {
        const needsScratch = chips.filter((c) => NN_OPTIMIZED_CHIPS.includes(c))
        if (scratch === 0 && needsScratch.length > 0 && base > 8) {
          // A non-trivial model on an esp-nn chip with no scratch declared is
          // the exact shape of the person-detect bug: correct on esp32/c3,
          // 60 KB short on esp32s3. Small arenas (the FullyConnected reference
          // model) genuinely need none, hence the base > 8 guard.
          warnings.push({
            level: 'warning',
            ...ctx,
            message:
              `${inst.block} (prefix=${inst.prefix}): arena_scratch_kb is 0 but ${needsScratch.join('/')} ` +
              `compile esp-nn's optimized kernels, which allocate a per-model scratch buffer on top of the ` +
              `base arena. Upstream's person-detection graph declares 60 KB. Measure it with ` +
              `scripts/bench-arena.sh rather than guessing — too small fails AllocateTensors at runtime, not at build.`,
          })
        }
      }

      if (Number(cfgOr('arena_in_psram') ?? 0) === 1 && chips.length > 0) {
        const noPsram = chips.filter((c) => !PSRAM_CHIPS.includes(c))
        if (noPsram.length > 0) {
          warnings.push({
            level: 'warning',
            ...ctx,
            message:
              `${inst.block} (prefix=${inst.prefix}): arena_in_psram is 1 but ${noPsram.join('/')} has no PSRAM. ` +
              `The allocation falls back to internal RAM silently, so the arena is spent from the scarcest ` +
              `pool on the part least able to afford it.`,
          })
        }
      }
    }
  }

  const seenPrefixes = new Set<string>()
  const paramMap = new Map<string, ParamRef>()

  for (const inst of product.instances) {
    if (!inst.block) {
      errors.push({ level: 'error', ...ctx, message: "instance missing 'block' field" })
      continue
    }
    const block = blocks.get(inst.block)
    if (!block) {
      errors.push({
        level: 'error',
        ...ctx,
        message: `instance references unknown block '${inst.block}'`,
      })
      continue
    }

    if (frameworks.includes('matter') && block.matter_wiring === 'endpoint_only') {
      warnings.push({
        level: 'warning',
        ...ctx,
        message:
          `${inst.block} is Matter endpoint-only: the device appears on the fabric but ` +
          `reflects and accepts no state over Matter. Its driver_params work over the ` +
          `other selected frameworks.` +
          (block.matter_wiring_note ? ` To complete it: ${block.matter_wiring_note}.` : ''),
      })
    }

    if (!inst.prefix) {
      errors.push({ level: 'error', ...ctx, message: `instance of ${inst.block} has no prefix` })
    } else if (!/^[A-Z][A-Z0-9_]*$/.test(inst.prefix)) {
      errors.push({
        level: 'error',
        ...ctx,
        message: `prefix '${inst.prefix}' must match /^[A-Z][A-Z0-9_]*$/`,
      })
    } else {
      const key = `${inst.block}@${inst.prefix}`
      if (seenPrefixes.has(key)) {
        errors.push({ level: 'error', ...ctx, message: `duplicate (block,prefix) pair: ${key}` })
      }
      seenPrefixes.add(key)
    }

    const cfg = inst.cfg ?? {}

    for (const [paramName, schema] of Object.entries(block.params ?? {})) {
      if (cfg[paramName] === undefined && schema.default === undefined && schema.required) {
        errors.push({
          level: 'error',
          ...ctx,
          message: `${inst.block} (prefix=${inst.prefix}): missing required cfg.${paramName}`,
        })
      }
      if (schema.enum && cfg[paramName] !== undefined && !schema.enum.includes(String(cfg[paramName]))) {
        errors.push({
          level: 'error',
          ...ctx,
          message: `${inst.block}: cfg.${paramName}='${cfg[paramName]}' not in enum [${schema.enum.join(',')}]`,
        })
      }
    }

    for (const cfgKey of Object.keys(cfg)) {
      if (!block.params || !(cfgKey in block.params)) {
        errors.push({
          level: 'error',
          ...ctx,
          message: `${inst.block} (prefix=${inst.prefix}): unknown cfg.${cfgKey}`,
        })
      }
    }

    for (const [refKey, refType] of Object.entries(block.param_refs ?? {})) {
      const paramId = String(cfg[refKey] ?? block.params?.[refKey]?.default ?? '')
      if (!paramId) {
        // Optional param left empty = feature disabled (its {{#if cfg.X}} slot is
        // dropped at generation). Only a missing REQUIRED ref is an error.
        if (block.params?.[refKey]?.required !== true) continue
        errors.push({
          level: 'error',
          ...ctx,
          message: `${inst.block} (prefix=${inst.prefix}): param_refs.${refKey} unresolved`,
        })
        continue
      }
      if (!/^APP_DRIVER_PARAM_[A-Z0-9_]+$/.test(paramId)) {
        warnings.push({
          level: 'warning',
          ...ctx,
          message: `param ID '${paramId}' from ${inst.block}/${inst.prefix} doesn't match APP_DRIVER_PARAM_<NAME>`,
        })
      }
      let entry = paramMap.get(paramId)
      if (!entry) {
        entry = { name: paramId, type: refType, emittedBy: [], consumedBy: [] }
        paramMap.set(paramId, entry)
      } else if (entry.type !== refType) {
        errors.push({
          level: 'error',
          ...ctx,
          message: `param '${paramId}' conflicting types: ${entry.type} vs ${refType} (${inst.block}/${inst.prefix})`,
        })
      }
      if (block.kind === 'driver') entry.emittedBy.push(`${inst.block}/${inst.prefix}`)
      else if (block.kind === 'device_type') entry.consumedBy.push(`${inst.block}/${inst.prefix}`)
      else {
        entry.emittedBy.push(`${inst.block}/${inst.prefix}`)
        entry.consumedBy.push(`${inst.block}/${inst.prefix}`)
      }
    }
  }

  for (const ref of paramMap.values()) {
    if (ref.consumedBy.length === 0) {
      warnings.push({
        level: 'warning',
        ...ctx,
        message: `param '${ref.name}' published by [${ref.emittedBy.join(', ')}] but no device_type block consumes it`,
      })
    }
    if (ref.emittedBy.length === 0) {
      warnings.push({
        level: 'warning',
        ...ctx,
        message: `param '${ref.name}' consumed by [${ref.consumedBy.join(', ')}] but no driver publishes it`,
      })
    }
    // Two DEVICE-TYPE instances consuming the same param generate duplicate
    // `case` labels inside each selected framework's driver_cb switch — a
    // hard compile error the product author only sees at build time. ERROR
    // here so it's caught at validation. (Multiple non-device-type consumers
    // — behaviors, persistence — are fine; they render in separate files.)
    const deviceConsumers = ref.consumedBy.filter((c) => c.startsWith('device_types/'))
    if (deviceConsumers.length > 1) {
      errors.push({
        level: 'error',
        ...ctx,
        message: `param '${ref.name}' consumed by ${deviceConsumers.length} device-type instances [${deviceConsumers.join(', ')}] — duplicate case labels in every selected framework's driver_cb; give each instance its own param`,
      })
    }
  }

  // ── Chip/pin compatibility ─────────────────────────────────────────────
  // Products are chip-agnostic, so a GPIO choice can be fine on one chip and
  // an internal flash pin on another — a runtime fault no build catches
  // (learned on hardware: GPIO 15 is SPICLK on esp32c3 → silent TG1WDT
  // reboot loop, no backtrace). One compact warning per product.
  //
  // Deliberately checks the RISC-V devkit chips the CATALOG pin defaults
  // target — not every chip the studio picker offers. Classic esp32's flash
  // range (GPIO 6-11) overlaps most catalog defaults by construction, so
  // including it would flood every product with warnings; the same holds for
  // esp32p4, whose free pins are what the Function EV board's on-board
  // peripherals leave over. esp32/h2/p4 products rely on the agent flow
  // (get_module_gpio) assigning chip-valid pins rather than catalog defaults.
  const PIN_CHECK_CHIPS = ['esp32c3', 'esp32c5', 'esp32c6', 'esp32s3']
  /** Every GPIO claim, one entry per (instance, cfg key) — NOT deduped by pin,
   *  because the board check below exempts claims per INSTANCE (a step-aside
   *  display's pin 9 must not shadow another block's real claim on pin 9). */
  const pinClaims: Array<{ pin: number; block: string; prefix: string; key: string }> = []
  for (const inst of product.instances ?? []) {
    for (const [key, value] of Object.entries(inst.cfg ?? {})) {
      if (typeof value !== 'number' || !key.includes('gpio')) continue
      pinClaims.push({ pin: value, block: inst.block, prefix: inst.prefix, key })
    }
  }
  const seenPins = new Map<number, { block: string; prefix: string; key: string }>()
  for (const { pin, ...claim } of pinClaims) {
    if (!seenPins.has(pin)) seenPins.set(pin, claim)
  }
  const pinExclusions: string[] = []
  for (const [pin, { prefix, key }] of seenPins) {
    const src = `${prefix}.${key}`
    const bad = PIN_CHECK_CHIPS.filter((c) => {
      const caps = chipCapabilities(c)
      return caps !== undefined && !caps.usableGpios.includes(pin)
    })
    if (bad.length > 0) pinExclusions.push(`gpio ${pin} (${src}) unusable on ${bad.join('/')}`)
  }
  if (pinExclusions.length > 0) {
    warnings.push({
      level: 'warning',
      ...ctx,
      message: `chip/pin compatibility: ${pinExclusions.join('; ')}`,
    })
  }

  // ── Board (esp-board-manager) ──────────────────────────────────────────
  //
  // These checks are about a product ON HARDWARE, so they run only when a
  // board file is supplied. A catalog product on its own is general: it names
  // no board, and there is nothing here to say about it. (What there used to
  // be — "a board product must declare ci_chips" — no longer means anything:
  // ci_chips is CI's chip list for a general product, and the board is not the
  // product's to declare.)
  //
  // Runs IN ADDITION to the chip check above: the chip says which pins exist,
  // the board says which of them are already spoken for.
  const boardFile = boardOpts.board ?? null
  const bmgrBoard = bmgrBoardOf(boardFile)
  /** Resolved board directory, shared with the bus rules below. `null` with
   *  `boardUnresolved` set means "a board was named but we could not read it",
   *  which is a different answer from "no board" for anything that would
   *  otherwise report an error the board might have satisfied. */
  let boardDir: string | null = null
  let boardUnresolved = false
  if (boardFile) {
    const ciChips = product.ci_chips ?? []
    // The chips CI builds this product on and the chip the user's hardware
    // carries have to be the same silicon, or the build proves nothing about
    // the board it claims to be for.
    const wrongCi = ciChips.filter((c) => c !== boardFile.chip)
    if (wrongCi.length > 0) {
      errors.push({
        level: 'error',
        ...ctx,
        message: `ci_chips [${ciChips.join(', ')}] disagrees with the board file's chip (${boardFile.chip}, board '${boardFile.selected}')`,
      })
    }
  }
  if (bmgrBoard) {
    const root = boardsRoot(boardOpts)
    if (!fs.existsSync(root)) {
      boardUnresolved = true
      // Host content missing — same stance as a machine with no ESP-IDF.
      warnings.push({
        level: 'warning',
        ...ctx,
        message: `board '${bmgrBoard}' not checked: no board packs at ${root} (set ZC_BOARDS_DIR)`,
      })
    } else {
      boardDir = findBoardDir(root, bmgrBoard)
      if (!boardDir) {
        boardUnresolved = true
        errors.push({
          level: 'error',
          ...ctx,
          message: `board '${bmgrBoard}' not found — looked for '<pack>/${bmgrBoard}/board_info.yaml' under ${root} (set ZC_BOARDS_DIR to point at your board packs)`,
        })
      } else {
        // A board definition names its own silicon. Three statements about one
        // chip have to agree: the board file (what the user's hardware is),
        // the resolved definition, and ci_chips where the product declares it.
        const chip = boardChip(boardDir)
        if (chip === undefined) {
          warnings.push({
            level: 'warning',
            ...ctx,
            message: `board '${bmgrBoard}' has no readable 'chip:' in board_info.yaml (${boardDir}) — chip agreement not checked`,
          })
        } else if (boardFile && chip !== boardFile.chip) {
          errors.push({
            level: 'error',
            ...ctx,
            message: `board file says chip: ${boardFile.chip}, but its bmgr board '${bmgrBoard}' (${boardDir}/board_info.yaml) says chip: ${chip}${boardFile.bmgr?.resolved_from === 'fallback' ? ' — a fallback board must be the module for the SAME chip' : ''}`,
          })
        }

        // Pin legality against the BOARD: a product must not wire an external
        // part onto a pin the board already uses for its own hardware.
        //
        // Two exemptions, both INTENT rather than amnesty:
        //  - STEP-ASIDE (boardTakesOver — the generator's own predicate): the
        //    board carries the device this block drives, the block's mapped
        //    slots are dropped and its pins rendered nowhere, so "its pins
        //    collide with the board" is the composition working. On a board
        //    WITHOUT the device the block keeps its init and its pins are
        //    checked exactly as before.
        //  - ADAPTER BINDING: an instance whose bmgr mapping BINDS a board
        //    device by name (provided_by + device_param) is the product
        //    deliberately driving that device, so the pins that device claims
        //    stop being foreign — a companion block sharing them (a long-press
        //    behavior on the bound button's GPIO) is asking the owner, not
        //    fighting it.
        const boardDevs = boardDeclaredDevices(boardDir)
        const owners = boardPinOwners(boardDir)
        /** `type:name` of every board device the product binds via an adapter. */
        const boundDevices = new Set<string>()
        for (const inst of product.instances) {
          const b = blocks.get(inst.block)
          const dp = b?.bmgr?.device_param
          const ty = b?.bmgr?.provided_by
          if (!b || !dp || !ty) continue
          const want = String((inst.cfg ?? {})[dp] ?? '')
          if (want) boundDevices.add(`${ty}:${want}`)
        }
        /** Adapter blocks in the catalog able to drive a board device of
         *  `type` — the fix line for a conflict with a bindable device.
         *  Derived, not hardcoded: a new adapter shows up here by itself. */
        const adaptersFor = (type: string, deviceName: string): string[] =>
          [...blocks.entries()]
            .filter(([, b]) => b.bmgr?.device_param !== undefined && b.bmgr?.provided_by === type)
            .map(([id, b]) => `${id} (cfg.${b.bmgr!.device_param}: ${deviceName})`)
            .sort()
        const conflicts: string[] = []
        for (const { pin, block: blockId, prefix, key } of pinClaims) {
          const owner = owners.get(pin)
          if (!owner) continue
          const blk = blocks.get(blockId)
          if (blk && boardTakesOver(blk, boardDevs)) continue
          if (owner.device && boundDevices.has(`${owner.device.type}:${owner.device.name}`)) continue
          let fix = ''
          if (owner.device) {
            const adapters = adaptersFor(owner.device.type, owner.device.name)
            if (adapters.length > 0) {
              fix = ` — the board already has this ${owner.device.type} ('${owner.device.name}'): ` +
                `use ${adapters.join(' or ')} instead of wiring your own on gpio ${pin}`
            }
          }
          conflicts.push(`gpio ${pin} (block ${blockId}, ${prefix}.${key}) is used by the board's ${owner.desc}${fix}`)
        }
        if (conflicts.length > 0) {
          errors.push({
            level: 'error',
            ...ctx,
            message: `board '${bmgrBoard}' pin conflict: ${conflicts.join('; ')} — pick a free pin, or drop the block and use the board's own device`,
          })
        }
      }
    }
  }

  // ── Board-device adapters (bmgr.device_param) ──────────────────────
  //
  // A block that drives hardware the BOARD declares names it by cfg, and is
  // matched by TYPE + NAME against the board's own board_devices.yaml. Same
  // reasoning as the bus rules below: unchecked, the product generates, builds
  // and boots, and the only sign is one ESP_LOGE nobody is watching for. The
  // generator throws on all three failures; this is the same answer earlier,
  // where a whole catalog is checked at once instead of one generate at a time.
  product.instances.forEach((inst) => {
    const block = blocks.get(inst.block)
    const deviceParam = block?.bmgr?.device_param
    if (!block || !deviceParam) return
    const want = String((inst.cfg ?? {})[deviceParam] ?? '')
    const type = block.bmgr?.provided_by ?? ''
    const where = `${inst.block} (prefix=${inst.prefix})`
    if (!bmgrBoard) {
      errors.push({
        level: 'error',
        ...ctx,
        message: `${where} drives the board's own '${want}' device, but this product has no board — a block that binds a board device cannot run without one`,
      })
      return
    }
    if (!boardDir) {
      // An unreadable pack is host content missing, not a product defect — the
      // same stance every other board rule takes. The generator still refuses.
      if (boardUnresolved) {
        warnings.push({
          level: 'warning',
          ...ctx,
          message: `${where} wants board device '${want}' (type '${type}'); board '${bmgrBoard}' could not be read, so it was not checked`,
        })
      }
      return
    }
    const declared = boardDeclaredDevices(boardDir)
    if (!declared.some(d => d.name === want && d.type === type)) {
      const same = declared.filter(d => d.type === type).map(d => `'${d.name}'`)
      errors.push({
        level: 'error',
        ...ctx,
        message: `${where} wants board device '${want}' of type '${type}', which board '${bmgrBoard}' does not declare. Its ${type} devices are: ${same.join(', ') || '(none)'}`,
      })
    }
  })

  // ── Shared buses ───────────────────────────────────────────────────────
  //
  // A block declaring `requires_bus:` performs transactions on a bus it does
  // NOT configure (the eleven legacy `driver/i2c.h` sensors). Nothing used to
  // check that: such a product compiles, links, boots, and is silent on the
  // device — the single worst failure shape we have, because every gate the
  // pipeline runs passes.
  //
  // Two things are checked, and the second is the one that looks like a nit
  // and is not: init order is product.yml order (app_driver_init calls each
  // instance's driver_init in sequence), so a bus configured AFTER its first
  // transaction is the same bug as no bus at all.
  const providers: Array<{ type: string; port: string; at: number; who: string; block: string }> = []
  product.instances.forEach((inst, at) => {
    const block = blocks.get(inst.block)
    if (!block) return
    for (const decl of busDecls(block.provides_bus)) {
      providers.push({ type: decl.type, port: busPort(block, inst.cfg ?? {}, decl), at, who: `${inst.block} (prefix=${inst.prefix})`, block: inst.block })
    }
  })

  // PROVIDER-side collision with the BOARD (the mirror of the consumer rule
  // below). A consumer on a board-owned port adopts the board's bus; a
  // PROVIDER on that port is a second configuration of the same hardware —
  // the amend emits a second bus peripheral and esp_board_manager_init()
  // aborts on the second i2c_new_master_bus(). The product validates, builds
  // and dies at boot, so it has to be an error here. buildBoardAmend carries
  // the same guard for a product generated without validation.
  for (const p of providers) {
    if (boardDir) {
      const boardPeriph = boardProvidesBus(boardDir, p.type, p.port)
      if (boardPeriph) {
        errors.push({
          level: 'error',
          ...ctx,
          message:
            `${p.who} configures ${p.type} port ${p.port}, but board '${bmgrBoard}' already ` +
            `provides that bus as its peripheral '${boardPeriph}' — esp_board_manager_init() ` +
            `would create the bus twice and abort at boot. Drop the ${p.block} instance: the ` +
            `board provides port ${p.port}, and consumers on it adopt the board's bus automatically.`,
        })
      }
    } else if (boardUnresolved) {
      warnings.push({
        level: 'warning',
        ...ctx,
        message: `${p.who} configures ${p.type} port ${p.port}; board '${bmgrBoard}' could not be read, so a collision with a board-owned bus was not checked`,
      })
    }
  }

  /** Blocks in the catalog that could configure this bus, each with the cfg
   *  key that puts it on the right port — the fix line, so the error is
   *  actionable without reading the engine. Derived, not hardcoded: an SPI
   *  provider added tomorrow shows up here by itself. */
  const providerBlocksFor = (type: string, port: string): string[] =>
    [...blocks.entries()]
      .flatMap(([id, b]) => busDecls(b.provides_bus)
        .filter(d => d.type === type)
        .map(d => `${id} (cfg.${d.port_param}: ${port})`))
      .sort()

  product.instances.forEach((inst, at) => {
    const block = blocks.get(inst.block)
    if (!block) return
    for (const decl of busDecls(block.requires_bus)) {
      const port = busPort(block, inst.cfg ?? {}, decl)
      const where = `${inst.block} (prefix=${inst.prefix})`
      const matching = providers.filter(p => p.type === decl.type && p.port === port)
      const early = matching.filter(p => p.at < at)
      // A provider on the same port, earlier in instances:, is the whole
      // requirement. It used to be narrower — a provider whose bmgr mapping
      // handed the bus to esp_board_manager was REFUSED, because the board
      // manager creates the bus with driver/i2c_master.h and the consumers
      // still spoke the legacy driver/i2c.h, which cannot open a port the new
      // driver owns. All fourteen I2C blocks moved to the new API, so a
      // board-owned bus and a product-owned one are now the same bus to a
      // consumer and that guard is gone with the reason for it.
      if (early.length > 0) continue

      // A board's own bus counts: `board:` means on-board hardware comes from
      // the board definition, and esp_board_manager_init() runs before
      // app_driver_init(), so it is configured in time by construction.
      if (boardDir && boardProvidesBus(boardDir, decl.type, port)) continue
      if (boardUnresolved) {
        warnings.push({
          level: 'warning',
          ...ctx,
          message: `${where} needs ${decl.type} port ${port} configured and no instance provides it; board '${bmgrBoard}' could not be read, so it was not checked`,
        })
        continue
      }

      const late = matching[0]
      const add = providerBlocksFor(decl.type, port)
      const fix = add.length > 0
        ? `add an instance of ${add.join(' or ')} EARLIER in instances:`
        : `add a block that configures ${decl.type} port ${port} earlier in instances:`
      errors.push({
        level: 'error',
        ...ctx,
        message: late
          ? `${where} talks on ${decl.type} port ${port}, which is configured by ${late.who} — but that instance comes LATER in instances: (position ${late.at + 1} vs ${at + 1}). Init order is product.yml order, so the first transaction runs against an unconfigured bus. Move the provider above it.`
          : `${where} talks on ${decl.type} port ${port} but never configures it, and no instance in this product does either — the build succeeds and the device is silent. Fix: ${fix}${bmgrBoard ? `, or use a board whose own ${decl.type} peripheral is on port ${port}` : ''}.`,
      })
    }
  })

  return { errors, warnings }
}

/** Format a list of errors for inclusion in a thrown Error message. */
export function formatErrors(errors: ValidationIssue[]): string {
  return errors
    .map(e => {
      const tag = e.product ? `[product ${e.product}]` : e.block ? `[block ${e.block}]` : ''
      return `  - ${tag} ${e.message}`.trim()
    })
    .join('\n')
}
