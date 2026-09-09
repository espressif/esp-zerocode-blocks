// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Internal types for the template/block system. Mirrors the YAML shapes
 * under firmware/templates/{code_blocks,product_configurations}.
 *
 * The agent-facing types live in `@esp-zerocode-ai/ai` (TemplateSummary,
 * ProductConfigurationDetail, CodeBlockDetail). These are the structural
 * counterparts used during loading + generation.
 */

export type ValueType = 'bool' | 'u8' | 'i16' | 'u16' | 'u32'

export const VALUE_TYPES: ValueType[] = ['bool', 'u8', 'i16', 'u16', 'u32']

export interface BlockParam {
  type: string
  required?: boolean
  default?: unknown
  enum?: string[]
  description?: string
}

export type BlockKind =
  | 'peripheral'
  | 'driver'
  | 'device_type'
  | 'binding'
  | 'behavior'
  | 'framework'
  | 'partition_table'
  | 'sdkconfig_fragment'

/** Plural directory name for a block kind. */
export const BLOCK_KIND_DIRS: Record<BlockKind, string> = {
  peripheral: 'peripherals',
  driver: 'drivers',
  device_type: 'device_types',
  binding: 'bindings',
  behavior: 'behaviors',
  framework: 'frameworks',
  partition_table: 'partition-tables',
  sdkconfig_fragment: 'sdkconfig-fragments',
}

/** Reverse map: directory name → kind. */
export const DIR_TO_BLOCK_KIND: Record<string, BlockKind> = Object.fromEntries(
  Object.entries(BLOCK_KIND_DIRS).map(([k, v]) => [v, k as BlockKind]),
) as Record<string, BlockKind>

/** One guard on a block's bmgr mapping.
 *
 *  A guard is a HARD gate, not a filter: a block declares the configurations
 *  its mapping is valid for, and an instance outside them FAILS generation
 *  with `reason` printed verbatim. That direction is deliberate. The mapping
 *  hands a physical line to code the block author did not write; the failure
 *  mode of getting it wrong is hardware doing something at boot (see the
 *  active-low note on drivers/relay), and a mapping that silently declined
 *  itself would look exactly like a block with no `bmgr:` at all. */
export interface BmgrGuard {
  /** cfg key whose value is checked (after block defaults are applied). */
  cfg: string
  /** The only value that permits the mapping. Compared by string form, so
   *  `1` and `"1"` are the same answer — YAML types vary across authors. */
  equals: unknown
  /** Why this is the only safe value. This IS the error message. */
  reason: string
}

/** A shared bus (I2C, SPI, …) a block either CONFIGURES or merely TALKS ON.
 *
 *  Eleven driver blocks perform I2C transactions against a port number they
 *  take as a param and never configure — they assume some other instance did.
 *  That assumption held only by convention: a product composed with such a
 *  sensor and no bus block builds clean, links clean, and is silent on the
 *  device with nothing in any report saying why. Declaring it makes the
 *  assumption checkable (see the bus rules in validator.ts).
 *
 *  `type` is the bus kind — the same word bmgr's peripheral `type:` uses
 *  (`i2c`, `spi`), so a board's own bus can satisfy a requirement. `port_param`
 *  names the cfg key holding the bus INDEX (I2C port, SPI host); two blocks
 *  are on the same bus when their resolved port_param values match. */
export interface BusDecl {
  /** Bus kind: `i2c`, `spi`, … Matched by string equality on both sides. */
  type: string
  /** cfg key holding the bus index. Resolved with the block's default. */
  port_param: string
}

/** How a block's pin params become esp-board-manager peripherals/devices in
 *  the amend overlay a `board:` product generates.
 *
 *  DECLARED, never inferred. A param-name heuristic (`*_gpio` → a gpio
 *  peripheral) would be writing pin claims and init code on a guess about
 *  hardware the block author knows and the engine does not, and a wrong guess
 *  produces a board definition that is confidently wrong — worse than no
 *  mapping at all. A block with no `bmgr:` contributes nothing to the amend
 *  and behaves exactly as it did. */
export interface BmgrMapping {
  /** Who configures and drives these lines at boot.
   *
   *  `board_manager` — esp-board-manager does, from the amend fragment. The
   *  slots named in `replaces_slots` are then dropped from the generated tree
   *  for a `board:` product, so the line has ONE owner.
   *
   *  A required word with (today) one legal value rather than an implied
   *  default: the point of this section is that the author states what takes
   *  the hardware over, so a future `init: block` (claim the pin, keep the
   *  block's own init) has to be written down, not fallen into. */
  init: 'board_manager'
  /** Slots this mapping takes over, dropped for a `board:` product. */
  replaces_slots?: string[]
  /** The board DEVICE TYPE that must already exist for this mapping to apply
   *  (a `type:` in the board's `board_devices.yaml` — `display_lcd`,
   *  `lcd_touch`, `audio_codec`, …).
   *
   *  Two opposite shapes of mapping share this section, and they need opposite
   *  defaults:
   *
   *  - **The block ADDS hardware the board does not have** (drivers/relay: a
   *    relay is wired TO a board, never on it). Its `peripherals:` go into the
   *    amend, so the board manager owns the line on ANY board. No
   *    `provided_by`, and `replaces_slots` applies whenever a board is present.
   *  - **The board ALREADY HAS the hardware** (the panel soldered to a room
   *    panel). Then the block has nothing to add — it must step aside so the
   *    board's own device is the ONE owner. But only on a board that really
   *    carries one: the same panel block wired externally to a bare devkit has
   *    to keep its own init, or the product silently loses its display.
   *
   *  `provided_by` is what tells those apart, and it is checked against the
   *  board's own YAML rather than assumed — the same "declared, never
   *  inferred" rule as the rest of this section. Absent = the first shape. */
  provided_by?: string
  /** The cfg key holding the NAME of the board device this block binds to —
   *  which makes the block a board-provided-hardware ADAPTER, a third shape
   *  beside the two above.
   *
   *  The display path (the board contributing `display_node_init`) works
   *  because a framework has a slot the board can fill. Most on-board hardware
   *  has no such framework: a board's own status LED is a `gpio_ctrl` device
   *  with a handle sitting there and nothing that fetches it. An adapter block
   *  is that fetcher — it drives hardware the board declares, through
   *  `esp_board_manager_get_device_handle`, and states NO pin of its own,
   *  which is the whole point: the pin belongs to the board, and claiming it
   *  in the amend is (correctly) an IO conflict.
   *
   *  Matched by TYPE (`provided_by`), fetched by NAME (this cfg key) — the
   *  same two-vocabulary rule the display adapter follows, and needed for the
   *  same reason plus one more: a board may carry SEVERAL devices of one type
   *  (the ESP-Mosaico has two `gpio_ctrl`s, a status LED and a vibration
   *  motor), so "the first one of that type" is not an answer here.
   *
   *  Declaring it makes the block BOARD-ONLY and CHECKED: generation fails if
   *  there is no board, if the board pack cannot be read, or if the board
   *  declares no device of that type under that name. Without the check the
   *  block still generates and builds and the failure is one log line at boot
   *  — the "compiles, links, boots, does nothing" shape this engine exists to
   *  refuse. */
  device_param?: string
  /** Configurations the mapping is valid for. All must hold. */
  requires?: BmgrGuard[]
  /** bmgr peripheral fragments. `{{cfg.x}}` / `{{prefix}}` / `{{prefix_lc}}`
   *  are substituted; a placeholder that is the WHOLE value keeps its type,
   *  so `pin: '{{cfg.gpio}}'` emits `pin: 4`, not `pin: '4'`.
   *
   *  SHARED RESOURCES. An entry may carry a reserved `shared_key:` (itself
   *  templated, e.g. `i2c-{{cfg.port}}`). The key is stripped from what is
   *  emitted and instead means "this fragment describes a resource several
   *  instances share": the FIRST instance resolving a key emits it, later ones
   *  resolving the SAME key emit nothing, and a later one resolving the same
   *  key to DIFFERENT YAML is an error naming both instances and the fields
   *  they disagree on. Without it, two sensors on one bus would each emit an
   *  `i2c` peripheral and bmgr's field-by-field merge would silently pick a
   *  winner. Nothing about the mechanism is I2C-specific — an SPI host, a
   *  shared LEDC timer or a shared UART use it the same way. */
  peripherals?: unknown[]
  /** bmgr device fragments, same substitution rules. */
  devices?: unknown[]
}

export interface Block {
  id: string
  kind: BlockKind
  description: string
  /** How finished the block is, as DATA rather than a word in the prose.
   *  `stub`: compiles but does nothing real (a periodic fake reading, an
   *  endpoint with no cluster logic). A map states it per framework — a
   *  device type whose RainMaker binding works while its Matter side is an
   *  endpoint-only stub is `{ matter: 'stub' }`. Absent = ready. */
  maturity?: 'stub' | Record<string, 'stub' | 'ready'>
  /** Optional esp-board-manager mapping — see BmgrMapping. */
  bmgr?: BmgrMapping
  /** Buses this block CONFIGURES, and which cfg key names each one. */
  provides_bus?: BusDecl | BusDecl[]
  /** Buses this block TALKS ON but does not configure — see BusDecl. */
  requires_bus?: BusDecl | BusDecl[]
  device_types?: Record<string, string>
  /** 'endpoint_only': the Matter endpoint exists but no attributes are wired —
   *  the device shows on the fabric and reflects/accepts nothing. Kept in step
   *  with the slots by scripts/check.py. */
  matter_wiring?: 'endpoint_only'
  /** What completing the Matter side takes (or why endpoint-only is final). */
  matter_wiring_note?: string
  idf_components?: Record<string, string | Record<string, unknown>>
  params?: Record<string, BlockParam>
  param_refs?: Record<string, ValueType>
  cmake_priv_requires?: Record<string, string[]>
  sdkconfig?: Record<string, string>
  /** Marks a `sdkconfig_fragment` block as a CHIP's per-chip base rather than a
   *  product-selectable fragment. The generator emits its `sdkconfig` into the
   *  selected chip's merged sdkconfig.defaults; a product must never list it in
   *  `sdkconfig_fragments` (products are chip-agnostic). The value is the chip
   *  target and must equal the block's directory name. */
  target?: string
  /** FRAMEWORK blocks only: the chips the framework has been built for. Read
   *  by CI to pick which framework x chip cells to compile; the platform's
   *  framework registry is kept in step with it. */
  chips?: string[]
  /** FRAMEWORK blocks only: whether the framework needs a radio (a transport,
   *  a mesh, a BLE link, the internet) or is entirely on-device (a screen, a
   *  speaker, a microphone). The generator drops the ESP32-P4's hosted radio
   *  stack from a tree whose frameworks all say false. */
  radio?: boolean
  slots?: Record<string, string>
}

export interface Instance {
  block: string
  prefix: string
  cfg: Record<string, unknown>
}

export interface Product {
  id: string
  name: string
  description: string
  keywords: string[]
  /** A bench fixture — proves a build path (board resolution, the amend
   *  overlay) rather than describing a product anyone would make. Listed to
   *  agents flagged as such, never presented as a peer of real products. */
  internal?: true
  /** Framework blocks this product composes (matter, rainmaker, audio, …). */
  frameworks: string[]
  instances: Instance[]
  /** Chips CI builds this product on. Absent = chip-agnostic (CI picks one).
   *  Predates the board work and is unrelated to it: it is CI's chip list, not
   *  a statement about anybody's hardware. */
  ci_chips?: string[]
  partition_table?: string
  sdkconfig_fragments?: string[]
  extra_sdkconfig?: Record<string, string>
  product_metadata?: { vendor_name?: string; product_name?: string }
}

/* ── the board: a property of an INSTANCE, never of the catalog ───────────
 *
 *  A product_configuration says what a product DOES. What hardware it runs on
 *  is the user's answer, not the catalog's: the same smart plug is a bare C6
 *  module for one person and an M5 NanoC6 for the next. So the board is a
 *  SEPARATE document (`board.yaml`, schema `zc-board/1`), handed to `generate`
 *  and to `validateProduct` beside the product — never a key inside
 *  product.yml.
 *
 *      schema: zc-board/1
 *      selected: esp32-c6-devkitc-1   # esp-virtual-parts board id
 *      chip: esp32c6
 *      bmgr:
 *        board: esp32_c6_devkitc_1    # resolved bmgr board dir, or null
 *        resolved_from: exact         # exact | fallback | custom
 */

/** How `bmgr.board` was arrived at.
 *
 *  `exact`    — the pack defines this very board.
 *  `fallback` — we resolved to a minimal `<chip>_module` board, i.e. we assert
 *               the CHIP and nothing else. The user's hardware is still what
 *               `selected` says; we simply have no definition of it.
 *  `custom`   — the definition travels in the product tree rather than being
 *               named from a pack. */
export type BoardResolution = 'exact' | 'fallback' | 'custom'

export const BOARD_RESOLUTIONS: BoardResolution[] = ['exact', 'fallback', 'custom']

/** The esp-board-manager half of a board file — how the hardware the user
 *  picked maps onto a board definition we can actually generate against. */
export interface BoardBmgr {
  /** Resolved bmgr board directory name, or null when no definition exists
   *  yet. Null is a COVERAGE fact about our packs, not a statement about the
   *  user's hardware, and it degrades to the chip-agnostic path — never to an
   *  error. */
  board: string | null
  resolved_from: BoardResolution
}

/** A parsed `board.yaml` — what hardware one product INSTANCE runs on. */
export interface BoardFile {
  /** `zc-board/1`. */
  schema?: string
  /** What the user picked: an esp-virtual-parts board id. ALWAYS present —
   *  hardware is chosen even when it is a bare module. */
  selected: string
  /** The silicon, as the studio/whiteboard knows it (`esp32c6`). */
  chip: string
  bmgr?: BoardBmgr
}

export const BOARD_SCHEMA = 'zc-board/1'

/** The bmgr board a board file resolves to, or null (no board file, no bmgr
 *  section, or no definition). The ONE place the rest of the engine asks
 *  "is there a board definition to generate against?". */
export function bmgrBoardOf(board: BoardFile | null | undefined): string | null {
  const name = board?.bmgr?.board
  return typeof name === 'string' && name.length > 0 ? name : null
}

/** A board device as (name, type) — the pair adapters and step-aside mappings
 *  are matched against. Structurally the generator's BoardDevice; declared
 *  here so the shared predicate below has no import cycle. */
export interface BoardDeviceRef {
  name: string
  type: string
}

/** THE step-aside test: does the board's own definition already carry the
 *  hardware this block drives, so the block must get out of the way?
 *
 *  ONE function on purpose, used by both the generator (renderInstances drops
 *  the mapping's `replaces_slots` when this holds) and the validator (the
 *  board pin-conflict check exempts such a block's pins — they are rendered
 *  nowhere, so a "collision" with the board is the composition working).
 *  These two used to be separate re-derivations, and they diverged: the
 *  generator stepped a hand-pinned display aside while the validator reported
 *  an 11-pin conflict wall for the same product + board.
 *
 *  False for a mapping with no `provided_by`: that shape ADDS hardware the
 *  board does not have (a relay wired TO the board), its pins go into the
 *  amend as real claims, and they stay inside the conflict check. */
export function boardTakesOver(block: Block, boardDevices: BoardDeviceRef[]): boolean {
  const providedBy = block.bmgr?.provided_by
  if (block.bmgr?.init !== 'board_manager' || providedBy === undefined) return false
  return boardDevices.some(d => d.type === providedBy)
}

export interface RenderedInstance {
  block: Block
  prefix: string
  cfg: Record<string, unknown>
  slots: Record<string, string>
  /** Slots the board manager took over (bmgr.replaces_slots on a board that
   *  carries the device) — dropped from `slots`, named here so the generated
   *  code can say where that init went instead of silently having none. */
  replacedSlots?: string[]
}
