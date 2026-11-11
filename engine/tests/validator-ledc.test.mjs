// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Two LEDC users on one timer at different frequencies, or on one channel, are
// refused; sharing a timer at one frequency is not.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { ledcConflicts } from '../dist/validator.js'

const blocks = new Map([
  ['drivers/servo_pwm', { params: { ledc_channel: { default: 4 }, ledc_timer: { default: 3 } }, slots: { driver_init: 'freq_hz = 50;' } }],
  ['drivers/dc_motor_h_bridge', { params: { ledc_channel: { default: 5 }, ledc_timer: { default: 3 }, freq_hz: { default: 20000 } }, slots: {} }],
  ['drivers/pwm_led', { params: { ledc_channel: { default: 0 }, ledc_timer: { default: 0 }, freq_hz: { default: 5000 } }, slots: {} }],
  ['drivers/pwm_rgb', { params: { red_channel: { default: 0 }, green_channel: { default: 1 }, blue_channel: { default: 2 }, ledc_timer: { default: 0 }, freq_hz: { default: 5000 } }, slots: {} }],
  ['drivers/buzzer', { params: { ledc_channel: { default: 3 }, ledc_timer: { default: 2 } }, slots: { driver_apply_cases: 'ledc_set_freq(LEDC_LOW_SPEED_MODE, t, f);' } }],
  ['drivers/relay', { params: { gpio: {} }, slots: {} }],
])
const inst = (block, prefix, cfg = {}) => ({ block, prefix, cfg })

test('servo + motor at their defaults share timer 3 at 50 Hz vs 20 kHz', () => {
  const out = ledcConflicts([inst('drivers/servo_pwm', 'S'), inst('drivers/dc_motor_h_bridge', 'M')], blocks)
  assert.equal(out.length, 1)
  assert.match(out[0], /LEDC timer 3 shared by drivers\/servo_pwm\/S .* and drivers\/dc_motor_h_bridge\/M \(20000 Hz\)/)
  assert.deepEqual(ledcConflicts([inst('drivers/servo_pwm', 'S'), inst('drivers/dc_motor_h_bridge', 'M', { ledc_timer: 1 })], blocks), [])
})

test('a timer shared at one frequency, and two instances of a fixed-frequency block, pass', () => {
  assert.deepEqual(ledcConflicts([inst('drivers/pwm_led', 'A', { ledc_channel: 4 }), inst('drivers/pwm_rgb', 'B')], blocks), [])
  assert.deepEqual(ledcConflicts([inst('drivers/servo_pwm', 'A'), inst('drivers/servo_pwm', 'B', { ledc_channel: 6 })], blocks), [])
})

test('a block that retunes at run time shares its timer with nobody, not even itself', () => {
  const out = ledcConflicts([inst('drivers/buzzer', 'A'), inst('drivers/buzzer', 'B', { ledc_channel: 6 })], blocks)
  assert.equal(out.length, 1)
  assert.match(out[0], /retuned at run time/)
})

test('two outputs on one channel are refused, including a multi-channel block', () => {
  const out = ledcConflicts([inst('drivers/pwm_led', 'A'), inst('drivers/pwm_rgb', 'B')], blocks)
  assert.equal(out.length, 1)
  assert.match(out[0], /LEDC channel 0 used by both drivers\/pwm_led\/A.ledc_channel and drivers\/pwm_rgb\/B.red_channel/)
})

test('blocks without an ledc_timer param are not LEDC users', () => {
  assert.deepEqual(ledcConflicts([inst('drivers/relay', 'R'), inst('drivers/relay', 'Q')], blocks), [])
})
