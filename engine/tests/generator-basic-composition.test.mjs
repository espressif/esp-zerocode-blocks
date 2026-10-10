// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// A framework on its own, with no product blocks yet, generates and links.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { genAppWebuiCpp, genAppAudioCpp } from '../dist/generator.js'

function deviceType(id) {
  return { block: { id, kind: 'device_type', slots: {} }, prefix: 'DT', cfg: {}, slots: {} }
}

test('webui with no device types generates (nothing to control yet)', () => {
  assert.doesNotThrow(() => genAppWebuiCpp([]))
})

test('webui with a device type that contributes no row is still refused', () => {
  assert.throws(() => genAppWebuiCpp([deviceType('device_types/on_off_light')]), /webui_entities row/)
})

test('audio has a weak microphone default, so it links without a mic block', () => {
  const out = genAppAudioCpp([])
  assert.match(out, /__attribute__\(\(weak\)\) size_t zc_audio_mic_read\(int16_t \*dest, size_t samples\)/)
})

test('audio with a microphone block has no weak default (one translation unit: it would be a redefinition)', () => {
  const mic = {
    block: { id: 'drivers/voice_mic_i2s', kind: 'driver', slots: {} }, prefix: 'MIC', cfg: {},
    slots: { audio_statics: 'size_t zc_audio_mic_read(int16_t *dest, size_t samples)\n{\n    return samples;\n}' },
  }
  const out = genAppAudioCpp([mic])
  assert.equal(out.match(/size_t zc_audio_mic_read\(/g)?.length, 1)
  assert.doesNotMatch(out, /__attribute__\(\(weak\)\)/)
})

test('a block that only calls zc_audio_mic_read still gets the weak default', () => {
  const caller = {
    block: { id: 'behaviors/x', kind: 'behavior', slots: {} }, prefix: 'X', cfg: {},
    slots: { audio_statics: 'static size_t peek(void) { int16_t b[4]; return zc_audio_mic_read(b, 4); }' },
  }
  assert.match(genAppAudioCpp([caller]), /__attribute__\(\(weak\)\) size_t zc_audio_mic_read/)
})
