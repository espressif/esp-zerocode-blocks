// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// @esp-zerocode-ai/firmware-engine — the portable core that turns blocks +
// base_firmware into a validated, buildable firmware tree. No host/agent/cloud
// deps (chipCapabilities is vendored) so it runs standalone in this repo AND is
// consumed by the ZeroCode platform via a pinned git dependency.
export * from './types.js'
export * from './loader.js'
export * from './validator.js'
export * from './generator.js'
export { chipCapabilities, normalizeChipTarget, type ChipCapabilities } from './hardware.js'
