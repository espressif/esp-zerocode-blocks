// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Device data: the per-device values a product's firmware reads from the
 * `fctry` NVS partition, declared by its blocks (`device_data:`, see
 * DeviceDataDecl) and written by the generator as `device_data.csv` at the
 * tree root.
 *
 * The CSV is an `nvs_partition_gen` input with the values left blank, plus
 * `label` and `required` columns that generator ignores. The platform builds
 * its flash form from it and refuses to publish one that breaks its rules, so
 * the checks here mirror those rules: a declaration that passes here produces
 * a file the platform accepts.
 */

import type { Block, DeviceDataKey } from './types.js'

export const DEVICE_DATA_FILE = 'device_data.csv'
export const DEVICE_DATA_PARTITION = 'fctry'

/** NVS keys and namespace names are at most 15 bytes. */
const NVS_NAME_MAX = 15
/** Stricter than NVS: a name that starts a CSV line must never read as a
 *  `#` comment or need quoting. */
const NVS_NAME = /^[A-Za-z0-9_]+$/

const PRIMITIVE_ENCODINGS = ['u8', 'i8', 'u16', 'i16', 'u32', 'i32', 'u64', 'i64']
const VARLEN_ENCODINGS = ['string', 'binary', 'hex2bin', 'base64']

const INT_RANGES: Record<string, [bigint, bigint]> = {
  u8: [0n, 0xffn],
  i8: [-0x80n, 0x7fn],
  u16: [0n, 0xffffn],
  i16: [-0x8000n, 0x7fffn],
  u32: [0n, 0xffffffffn],
  i32: [-0x80000000n, 0x7fffffffn],
  u64: [0n, 0xffffffffffffffffn],
  i64: [-0x8000000000000000n, 0x7fffffffffffffffn],
}

/** Keys nvs_partition_gen rewrites into Wi-Fi entries rather than storing. */
const GENERATOR_SPECIAL_KEYS = new Set(['ap.ssid', 'ap.passwd', 'ap.authmode', 'sta.ssid', 'sta.pswd'])

const PRIVATE_KEY_PEM = /-----BEGIN [A-Z0-9 ]*PRIVATE KEY-----/

/** One row of a product's device data, after merging every block's
 *  declaration. */
export interface DeviceDataRow {
  namespace: string
  key: string
  type: 'data' | 'file'
  encoding: string
  default?: string
  label: string
  required: boolean
  /** The blocks that declare it, in product order. */
  from: string[]
}

function nameProblem(what: string, name: unknown): string | null {
  if (typeof name !== 'string' || name === '') return `${what} must be a non-empty string`
  if (!NVS_NAME.test(name)) return `${what} '${name}' may use only letters, digits and '_'`
  if (name.length > NVS_NAME_MAX) return `${what} '${name}' is longer than ${NVS_NAME_MAX} bytes`
  return null
}

function keyProblems(k: DeviceDataKey): string[] {
  const out: string[] = []
  const at = `device_data key '${k.key}'`
  const type = k.type ?? 'data'
  if (type !== 'data' && type !== 'file') out.push(`${at}: type must be data or file, not '${String(k.type)}'`)
  const encodings = type === 'file' ? VARLEN_ENCODINGS : [...PRIMITIVE_ENCODINGS, ...VARLEN_ENCODINGS]
  if (!encodings.includes(k.encoding)) {
    out.push(`${at}: encoding '${String(k.encoding)}' is not one of ${encodings.join(', ')} for a ${type} key`)
  }
  if (k.label !== undefined && (typeof k.label !== 'string' || /[\r\n]/.test(k.label))) {
    out.push(`${at}: label must be a single-line string`)
  }
  if (k.required !== undefined && typeof k.required !== 'boolean') out.push(`${at}: required must be true or false`)
  if (k.default === undefined) return out
  if (type === 'file') {
    out.push(`${at}: a file key takes no default — its bytes are always the user's, supplied at flash time`)
    return out
  }
  if (typeof k.default !== 'string' && typeof k.default !== 'number') {
    out.push(`${at}: default must be a string or a number`)
    return out
  }
  const v = String(k.default)
  if (/[\r\n]/.test(v)) out.push(`${at}: default must be a single line`)
  if (PRIVATE_KEY_PEM.test(v)) out.push(`${at}: default holds a private key — a declaration never holds a secret`)
  const range = INT_RANGES[k.encoding]
  if (range) {
    if (!/^-?\d+$/.test(v)) out.push(`${at}: default ${v} is not an integer`)
    else if (BigInt(v) < range[0] || BigInt(v) > range[1]) out.push(`${at}: default ${v} is out of range for ${k.encoding}`)
  }
  if (k.encoding === 'hex2bin' && !/^([0-9a-fA-F]{2})*$/.test(v)) out.push(`${at}: default is not an even-length hex string`)
  // nvs_partition_gen sizes a binary value by characters but stores its UTF-8
  // bytes, so only ASCII round-trips.
  if (k.encoding === 'binary' && !/^[\x00-\x7f]*$/.test(v)) out.push(`${at}: default must be ASCII for binary encoding`)
  return out
}

/** What is wrong with one block's `device_data:` declaration; [] when it is
 *  absent or sound. */
export function checkDeviceDataDecl(block: Block): string[] {
  const decl = block.device_data as unknown
  if (decl === undefined) return []
  if (!decl || typeof decl !== 'object' || Array.isArray(decl)) {
    return ['device_data must be a mapping: { namespace, keys }']
  }
  const { namespace, keys } = decl as { namespace?: unknown; keys?: unknown }
  const out: string[] = []
  const ns = nameProblem('device_data.namespace', namespace)
  if (ns) out.push(ns)
  if (!Array.isArray(keys) || keys.length === 0) {
    out.push('device_data.keys must be a non-empty list')
    return out
  }
  const seen = new Set<string>()
  for (const k of keys as unknown[]) {
    if (!k || typeof k !== 'object' || Array.isArray(k)) {
      out.push('device_data.keys entries must be mappings: { key, encoding, … }')
      continue
    }
    const key = (k as DeviceDataKey).key
    const kp = nameProblem('device_data key', key)
    if (kp) { out.push(kp); continue }
    if (GENERATOR_SPECIAL_KEYS.has(key)) out.push(`device_data key '${key}' is reserved by nvs_partition_gen`)
    if (seen.has(key)) out.push(`device_data key '${key}' is declared twice`)
    seen.add(key)
    out.push(...keyProblems(k as DeviceDataKey))
  }
  return out
}

function rowOf(namespace: string, k: DeviceDataKey, from: string): DeviceDataRow {
  return {
    namespace,
    key: k.key,
    type: k.type ?? 'data',
    encoding: k.encoding,
    ...(k.default !== undefined ? { default: String(k.default) } : {}),
    label: k.label ?? k.key,
    required: k.required ?? true,
    from: [from],
  }
}

function sameRow(a: DeviceDataRow, b: DeviceDataRow): boolean {
  return a.type === b.type && a.encoding === b.encoding && a.default === b.default &&
    a.label === b.label && a.required === b.required
}

/**
 * Every device data key the given blocks declare, merged into one list in
 * product order with each namespace's keys together (nvs_partition_gen files
 * a row under the most recently opened namespace, so a namespace is opened
 * once). The same key declared identically by two blocks is one row; declared
 * differently, it is an error naming both — they would be reading one value
 * with two meanings.
 */
export function collectDeviceData(blocks: Array<{ id: string; block: Block }>): { rows: DeviceDataRow[]; errors: string[] } {
  const errors: string[] = []
  const byNamespace = new Map<string, Map<string, DeviceDataRow>>()
  for (const { id, block } of blocks) {
    if (block.device_data === undefined) continue
    const problems = checkDeviceDataDecl(block)
    if (problems.length > 0) {
      errors.push(...problems.map(p => `${id}: ${p}`))
      continue
    }
    const { namespace, keys } = block.device_data
    const rows = byNamespace.get(namespace) ?? new Map<string, DeviceDataRow>()
    byNamespace.set(namespace, rows)
    for (const k of keys) {
      const row = rowOf(namespace, k, id)
      const held = rows.get(k.key)
      if (!held) { rows.set(k.key, row); continue }
      if (held.from.includes(id)) continue
      if (sameRow(held, row)) { held.from.push(id); continue }
      errors.push(
        `device data '${namespace}/${k.key}' is declared differently by ${held.from.join(', ')} and ${id} — ` +
        `blocks sharing a key must declare it identically (type, encoding, default, label, required)`,
      )
    }
  }
  return { rows: [...byNamespace.values()].flatMap(m => [...m.values()]), errors }
}

function csvField(v: string): string {
  return /[",]/.test(v) ? `"${v.replace(/"/g, '""')}"` : v
}

/** `device_data.csv` for the given rows. */
export function deviceDataCsv(rows: DeviceDataRow[]): string {
  const lines = [
    '# Device data this firmware reads from the fctry partition, generated from the',
    "# blocks' device_data declarations on every apply. Values stay blank: they are",
    '# entered in the browser at flash time and never stored with the firmware.',
    'key,type,encoding,value,label,required',
  ]
  let namespace: string | null = null
  for (const r of rows) {
    if (r.namespace !== namespace) {
      lines.push(`${r.namespace},namespace,,,,`)
      namespace = r.namespace
    }
    lines.push([r.key, r.type, r.encoding, r.default ?? '', r.label, r.required ? 'yes' : 'no'].map(csvField).join(','))
  }
  return `${lines.join('\n')}\n`
}
