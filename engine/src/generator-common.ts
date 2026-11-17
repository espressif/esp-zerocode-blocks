// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Helpers the generator and every framework's glue share.

import { promises as fs } from 'node:fs'
import * as path from 'node:path'
import type { RenderedInstance, ValueType } from './types.js'

export const VALUE_TYPE_TO_UNION_MEMBER: Record<ValueType, string> = {
  bool: 'b',
  u8: 'u8',
  i16: 'i16',
  u16: 'u16',
  u32: 'u32',
}

export function isFramework(r: RenderedInstance, name: string): boolean {
  return r.block.id === `frameworks/${name}`
}

/**
 * WHO BRINGS UP WI-FI, decided once for the whole product.
 *
 * Every framework that can own the radio initialises it in its own
 * app_<fw>_init(), and esp_wifi_init() refuses a second call
 * (ESP_ERR_INVALID_STATE → ESP_ERROR_CHECK abort). Each generator used to
 * derive its own `sharedNet` from its own list, and the lists disagreed:
 * [webui, agents] initialised Wi-Fi twice, and [webui, aws_iot] put the radio
 * in SoftAP mode, so aws_iot deferred to it and waited forever for a station
 * address. One precedence list, highest first; every framework but the first
 * one present runs in shared-network mode.
 *
 * aws_iot emits no generated code: it decides at RUNTIME, two seconds after
 * boot, and defers to anyone who initialised Wi-Fi synchronously. It sits
 * below every codegen owner for that reason, and above webui, whose
 * standalone SoftAP would leave it with no station to wait on.
 *
 * espnow also brings Wi-Fi up, but the registry refuses it beside every
 * framework in this list, so it is not part of the chain.
 */
export const WIFI_OWNER_PRECEDENCE = ['matter', 'rainmaker', 'mqtt', 'agents', 'aws_iot', 'webui'] as const

export function wifiOwner(rendered: RenderedInstance[]): string | undefined {
  return WIFI_OWNER_PRECEDENCE.find(fw => rendered.some(r => isFramework(r, fw)))
}

/** True when a framework other than `self` owns Wi-Fi in this product. */
export function wifiShared(rendered: RenderedInstance[], self: string): boolean {
  const owner = wifiOwner(rendered)
  return owner !== undefined && owner !== self
}

export function collectSlot(rendered: RenderedInstance[], slotName: string, indent = 0): string {
  const pad = ' '.repeat(indent)
  const parts = rendered
    .map(r => r.slots[slotName])
    .filter((s): s is string => Boolean(s && s.trim()))
  if (parts.length === 0) return ''
  return parts
    .map(p => p.replace(/^\n+|\n+$/g, '').split('\n').map(line => (line.length ? pad + line : line)).join('\n'))
    .join('\n\n')
}

/** What the catalog's base_firmware offers that generated code may call.
 *  The engine is released before the catalog that goes with it, so a newer
 *  engine meets an older catalog for a while; each flag is read from the
 *  catalog's own headers and the generated code falls back without it. */
export interface CatalogFeatures {
  /** app_driver.h declares app_driver_take_refusal (param guards). */
  takeRefusal: boolean
  /** app_utils.h declares app_utils_factory_reset (hook + whole-NVS erase). */
  utilsFactoryReset: boolean
}

export const CURRENT_CATALOG: CatalogFeatures = { takeRefusal: true, utilsFactoryReset: true }

export async function detectCatalogFeatures(baseFirmwareDir: string): Promise<CatalogFeatures> {
  const read = async (rel: string) => {
    try { return await fs.readFile(path.join(baseFirmwareDir, rel), 'utf8') } catch { return null }
  }
  const driverH = await read('components/app_driver/include/app_driver.h')
  const utilsH = await read('components/app_utils/include/app_utils.h')
  return {
    // A base without the header (a minimal fixture) is taken as current.
    takeRefusal: driverH === null || /\bapp_driver_take_refusal\s*\(/.test(driverH),
    utilsFactoryReset: utilsH === null || /\bapp_utils_factory_reset\s*\(/.test(utilsH),
  }
}

/** Without param guards nothing is ever refused: a stand-in that says so. */
export function refusalShim(features: CatalogFeatures): string {
  if (features.takeRefusal) return ''
  return `
/* This catalog's app_driver has no param guards, so no write is ever refused. */
static inline bool app_driver_take_refusal(app_driver_param_id_t *id)
{
    (void)id;
    return false;
}
`
}

export function collectParamIds(rendered: RenderedInstance[], productParams: Record<string, string> = {}): Array<{ name: string; type: ValueType }> {
  const map = new Map<string, ValueType>()
  for (const r of rendered) {
    for (const [paramName, valType] of Object.entries(r.block.owned_params ?? {})) {
      const prev = map.get(paramName)
      if (prev && prev !== valType) {
        throw new Error(`param ${paramName}: conflicting types ${prev} vs ${valType} (block ${r.block.id})`)
      }
      map.set(paramName, valType)
    }
  }
  for (const r of rendered) {
    for (const [cfgKey, declared] of Object.entries(r.block.param_refs ?? {})) {
      // A type may come from the instance's cfg (`{{cfg.value_kind}}`): a block
      // that serves ANY param (behaviors/webui_row) declares it with the type
      // the product chose.
      const tmpl = /^\{\{\s*cfg\.(\w+)\s*\}\}$/.exec(String(declared))
      const valType = (tmpl ? String(r.cfg[tmpl[1]!] ?? '') : declared) as ValueType
      if (!(valType in VALUE_TYPE_TO_UNION_MEMBER)) {
        throw new Error(`block ${r.block.id}: param_refs.${cfgKey} has type '${valType}' (expected one of ${Object.keys(VALUE_TYPE_TO_UNION_MEMBER).join(', ')})`)
      }
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
  // The product's own params (template.yaml `params:`): checked like a block's.
  for (const [paramName, declared] of Object.entries(productParams)) {
    if (!/^APP_DRIVER_PARAM_[A-Z0-9_]+$/.test(paramName)) {
      throw new Error(`product params: '${paramName}' is not a param name (APP_DRIVER_PARAM_<NAME>, upper case)`)
    }
    const valType = String(declared) as ValueType
    if (!(valType in VALUE_TYPE_TO_UNION_MEMBER)) {
      throw new Error(`product params: ${paramName} has type '${declared}' (expected one of ${Object.keys(VALUE_TYPE_TO_UNION_MEMBER).join(', ')})`)
    }
    const prev = map.get(paramName)
    if (prev && prev !== valType) {
      throw new Error(`param ${paramName}: the product declares ${valType}, a block declares it ${prev}`)
    }
    map.set(paramName, valType)
  }
  return [...map.entries()].map(([name, type]) => ({ name, type }))
}

export function dedent(s: string): string {
  const lines = s.split('\n')
  const indents = lines.filter(l => l.trim()).map(l => l.match(/^ */)![0].length)
  const min = indents.length ? Math.min(...indents) : 0
  return lines.map(l => l.slice(min)).join('\n')
}

export function indentText(s: string, n: number): string {
  const pad = ' '.repeat(n)
  return s.split('\n').map(l => (l.length ? pad + l : l)).join('\n')
}

/** Strip the outer `{ }` scope block a behavior's logic_init was authored with.
 *  That block only existed to isolate locals when every behavior's init was
 *  inlined into one app_logic_init(); as its own function it's redundant (and
 *  the bare blocks confuse editing). */
export function unwrapInitBody(slot: string): string {
  let s = slot.replace(/^\n+|\n+$/g, '')
  const t = s.trim()
  if (t.startsWith('{') && t.endsWith('}')) {
    const open = s.indexOf('{')
    const close = s.lastIndexOf('}')
    s = dedent(s.slice(open + 1, close)).replace(/^\n+|\n+$/g, '')
  }
  return s
}
