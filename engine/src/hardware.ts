// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Chip module definitions — devkit GPIO maps, features, and image references.
 * Sourced from esp_idc_web product-modules.json.
 */

export interface ChipModule {
  id: string
  chip: string
  displayName: string
  type: 'devkit' | 'module'
  gpios: number[]
  /** Radios ON THIS DIE. Empty on parts that have none (esp32p4). */
  features: ('wifi' | 'ble' | 'thread' | 'zigbee')[]
  /** Radios the firmware can use anyway, served by a companion chip over
   *  ESP-Hosted instead of by this die (esp32p4 + its ESP32-C6). Listed
   *  separately because they cost a second chip on the board and its slave
   *  firmware — never fold them into `features`. */
  hostedFeatures?: ('wifi' | 'ble' | 'thread' | 'zigbee')[]
  image: string
}

/** Default devkit per chip family */
export const CHIP_MODULES: Record<string, ChipModule> = {
  esp32: {
    id: 'ESP32-DevKitC',
    chip: 'esp32',
    displayName: 'ESP32-DevKitC',
    type: 'devkit',
    gpios: [0, 2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39],
    features: ['wifi', 'ble'],
    image: '/assets/devkits/esp32.png',
  },
  esp32c3: {
    id: 'ESP32-C3-DevKitM-1',
    chip: 'esp32c3',
    displayName: 'ESP32-C3-DevKitM-1',
    type: 'devkit',
    gpios: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 18, 19, 20, 21],
    features: ['wifi', 'ble'],
    image: '/assets/devkits/esp32c3.png',
  },
  esp32c5: {
    id: 'ESP32-C5-DevKitC-1',
    chip: 'esp32c5',
    displayName: 'ESP32-C5-DevKitC-1',
    type: 'devkit',
    // 0-28 exist; 15-22 are the SPI flash / PSRAM pads (SPICS0/1, SPIQ,
    // SPIWP, VDD_SPI, SPIHD, SPICLK, SPID).
    gpios: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 23, 24, 25, 26, 27, 28],
    features: ['wifi', 'ble', 'thread', 'zigbee'],
    image: '/assets/devkits/esp32c5.png',
  },
  esp32c6: {
    id: 'ESP32-C6-DevKitC-1',
    chip: 'esp32c6',
    displayName: 'ESP32-C6-DevKitC-1',
    type: 'devkit',
    gpios: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 18, 19, 20, 21, 22, 23],
    features: ['wifi', 'ble', 'thread', 'zigbee'],
    image: '/assets/devkits/esp32c6.png',
  },
  esp32h2: {
    id: 'ESP32-H2-DevKitM-1',
    chip: 'esp32h2',
    displayName: 'ESP32-H2-DevKitM-1',
    type: 'devkit',
    gpios: [0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14, 22, 23, 24, 25, 26, 27],
    features: ['ble', 'thread', 'zigbee'],
    image: '/assets/devkits/esp32h2.png',
  },
  esp32p4: {
    id: 'ESP32-P4-Function-EV-Board',
    chip: 'esp32p4',
    displayName: 'ESP32-P4-Function-EV-Board',
    type: 'devkit',
    // 0-54 exist and the flash/PSRAM sit on dedicated MSPI pads, so most
    // exclusions here are board-level, not chip-level: 14-19 are the SDIO
    // link to the on-board ESP32-C6 companion radio and 54 its reset line
    // (esp_hosted's defaults for this board), 39-44 the microSD slot; 24/25
    // are USB-Serial-JTAG, 26/27 the USB-OTG port and 32-38 strapping +
    // UART0 console, which are chip-level.
    gpios: [
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 20, 21, 22, 23,
      28, 29, 30, 31, 45, 46, 47, 48, 49, 50, 51, 52, 53,
    ],
    // Not a radio on the die: no Wi-Fi, no BLE, no 802.15.4. Both come from
    // the companion ESP32-C6 over ESP-Hosted — Wi-Fi through esp_wifi_remote,
    // BLE as HCI to its controller. 802.15.4 stays off the list: the companion
    // has the radio, but no block here drives Thread or Zigbee over hosted.
    features: [],
    hostedFeatures: ['wifi', 'ble'],
    image: '/assets/devkits/esp32p4.png',
  },
  esp32s31: {
    id: 'ESP32-S31-Function-CoreBoard-1',
    chip: 'esp32s31',
    displayName: 'ESP32-S31-Function-CoreBoard-1',
    type: 'devkit',
    // The devkit-style S31 board (family default follows the plain-devkit
    // convention; the ESP-Mosaico product board has its own pin map, selected
    // by name platform-side). GPIOs are the header pins the CoreBoard exposes
    // (user guide); 60/61 are strapping (61 = BOOT, 60 also drives the
    // onboard addressable RGB LED).
    gpios: [0, 1, 2, 3, 4, 35, 36, 37, 38, 39, 40, 42, 43, 44, 45, 46, 47, 48, 49, 60, 61],
    features: ['wifi', 'ble', 'thread', 'zigbee'],
    image: '/assets/devkits/esp32s31.png',
  },
  esp32s3: {
    id: 'ESP32-S3-DevKitC-1',
    chip: 'esp32s3',
    displayName: 'ESP32-S3-DevKitC-1',
    type: 'devkit',
    gpios: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48],
    features: ['wifi', 'ble'],
    image: '/assets/devkits/esp32s3.png',
  },
}

/**
 * Normalize any chip / module string to a valid ESP-IDF build target.
 * Handles devkit + module names the product_description agent can emit, e.g.
 * "ESP32-C3-WROOM-02" → "esp32c3", "ESP32-S3" → "esp32s3", "esp32" → "esp32".
 * Returns `fallback` when no esp32 family token is found.
 */
export function normalizeChipTarget(chip: string, fallback = 'esp32c3'): string {
  if (!chip) return fallback
  const s = chip.toLowerCase().replace(/[^a-z0-9]/g, '')
  // Longest family token FIRST, always. The old pattern was a single-digit
  // variant (`c\d|s\d|h\d|p\d`), which matched "esp32s3" INSIDE "esp32s31"
  // and "esp32c6" inside "esp32c61" — so an S31 product was generated, pin-
  // checked and BUILT as an S3. Silent, and wrong in exactly the way that is
  // hardest to notice: the build succeeds.
  //
  // A greedy `c\d+|s\d+|h\d+|p\d+` also fixes the S31 case and needs no
  // list, but it swallows digits that are not part of the family token: the
  // spec agent emits strings like "esp32c3 4mb" and "ESP32-C2 16MB", which
  // greedy turns into "esp32c34" and "esp32c216". An explicit list cannot do
  // that, so it wins despite needing an edit per new part. Mirrors the copy in
  // the monorepo's shared/src/hardware/modules.ts — keep the two in step.
  const m = s.match(/esp32(c61|h21|s31|c2|c3|c5|c6|h2|h4|p4|s2|s3)?/)
  if (!m) return fallback
  return 'esp32' + (m[1] ?? '')
}

/** Get the default module for a target chip */
export function getDefaultModule(targetChip: string): ChipModule | undefined {
  // Normalize: "esp32-c6" → "esp32c6", "ESP32C6" → "esp32c6"
  const normalized = targetChip.toLowerCase().replace(/-/g, '')
  return CHIP_MODULES[normalized] ?? CHIP_MODULES[normalizeChipTarget(targetChip, '')]
}

/** Per-chip radio capabilities — a fixed property of the silicon, not the
 *  product. Recorded in `product_details` so agents stop re-deriving (and
 *  mis-guessing) which transport a chip supports: ESP32-C3/S3/ESP32 are
 *  Wi-Fi only (no 802.15.4 → no Thread), ESP32-H2 is Thread only (no Wi-Fi),
 *  ESP32-C6 and ESP32-C5 support both (C5 on 2.4 and 5 GHz), ESP32-P4 has no
 *  radio of any kind and borrows a companion chip's over ESP-Hosted.
 *  `matterTransport` is the operational Matter network(s) — empty when the
 *  chip has no BLE to commission over, as on ESP32-P4. */
export interface ChipCapabilities {
  wifi: boolean
  ble: boolean
  thread: boolean
  zigbee: boolean
  matterTransport: ('wifi' | 'thread')[]
  /** GPIOs safe to assign on the default devkit (flash/PSRAM pins already
   *  excluded). The agent must pick only from these. */
  usableGpios: number[]
  /** Boot-strapping pins: within usableGpios, but their level is sampled at
   *  reset — avoid for inputs/outputs that may be driven during boot, or
   *  leave free when possible. */
  strappingGpios: number[]
  /** Set when NONE of the radios above are on this die — they belong to a
   *  companion chip reached over ESP-Hosted (esp32p4). The firmware talks to
   *  the same esp_wifi API, but the product needs that second chip on the
   *  board and its slave firmware flashed. */
  radioVia?: 'hosted'
}

/** Boot-strapping GPIOs per chip family (sampled at reset; ESP-IDF TRM /
 *  datasheets). Subset of usableGpios — flagged so the agent doesn't put a
 *  button/relay on a pin that's driven at boot. */
const STRAPPING_GPIOS: Record<string, number[]> = {
  esp32: [0, 2, 5, 12, 15],
  esp32c3: [2, 8, 9],
  esp32c5: [2, 3, 25, 26, 27, 28],
  esp32c6: [4, 5, 8, 9, 15],
  esp32h2: [8, 9, 25],
  esp32p4: [32, 33, 34, 35, 36, 37, 38],
  esp32s3: [0, 3, 45, 46],
  // Union of both readings, per the C5 precedent: over-flagging costs an agent
  // one pin of choice, under-flagging costs a board that will not boot.
  // esptool's boot-mode docs give 60/61 (61 = BOOT selects the ROM serial
  // bootloader, 60 must be high to enter it reliably). ESP-IDF's own
  // gpio/esp32s31.inc table AND register/soc/io_mux_reg.h `// Strapping:`
  // annotations agree with each other on five more: LDO sel (36), USB2JTAG
  // select (37) and boot-mode selects 0-2 (38, 39, 40). All seven are header
  // pins on the CoreBoard, so leaving 36-40 unflagged would offer them
  // silently.
  esp32s31: [36, 37, 38, 39, 40, 60, 61],
}

export function chipCapabilities(chip?: string | null): ChipCapabilities | undefined {
  const mod = chip ? getDefaultModule(chip) : undefined
  if (!mod) return undefined
  const hosted = mod.hostedFeatures ?? []
  // What the firmware can use, wherever the radio physically lives.
  const f = [...mod.features, ...hosted]
  const usable = mod.gpios
  return {
    wifi: f.includes('wifi'),
    ble: f.includes('ble'),
    thread: f.includes('thread'),
    zigbee: f.includes('zigbee'),
    // Matter always commissions over BLE, so no BLE means no Matter at all —
    // not "Matter over Wi-Fi only".
    matterTransport: f.includes('ble')
      ? f.filter((x): x is 'wifi' | 'thread' => x === 'wifi' || x === 'thread')
      : [],
    usableGpios: usable,
    strappingGpios: (STRAPPING_GPIOS[mod.chip] ?? []).filter((p) => usable.includes(p)),
    ...(mod.features.length === 0 && hosted.length > 0 ? { radioVia: 'hosted' as const } : {}),
  }
}
