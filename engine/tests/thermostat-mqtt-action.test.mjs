// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// device_types/thermostat's MQTT binding: with heating_param, the Home
// Assistant climate entity carries an action topic (heating / idle / off)
// driven by the heat output and the mode. The block's mqtt slots are rendered
// as the generator renders them, compiled on the host over a stubbed client,
// and every publish is printed for the assertions below.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'
import { __substituteForTest as substitute } from '../dist/generator.js'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const CXX = process.env.CXX || 'c++'
const haveCxx = spawnSync(CXX, ['--version']).status === 0

function slot(name, cfg) {
  const ctx = { cfg, prefix: 'THERMO', prefix_lc: 'thermo' }
  return substitute(readFileSync(join(ROOT, 'code_blocks/device_types/thermostat/slots', name), 'utf-8'), ctx)
}

const STUBS = String.raw`
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;
enum { P_TEMP, P_SET, P_MODE, P_HEAT, P_MAX };
typedef int app_driver_param_id_t;
typedef union { bool b; uint8_t u8; uint16_t u16; int16_t i16; uint32_t u32; } app_driver_param_val_t;
static app_driver_param_val_t g_param[P_MAX];
static bool g_seen[P_MAX];
static esp_err_t app_driver_get_param(int id, app_driver_param_val_t *v) { *v = g_param[id]; return 0; }
static bool app_driver_param_seen(int id) { return g_seen[id]; }
static esp_err_t app_driver_set_param(int id, app_driver_param_val_t v, void *) { g_param[id] = v; g_seen[id] = true; return 0; }
static const char *s_node_id = "abc123";
static const char *s_status_topic = "zerocode/abc123/status";
static void *s_client = NULL;
static void *s_handle = NULL;
static int esp_mqtt_client_subscribe(void *, const char *, int) { return 0; }
static int zc_mqtt_publish(const char *topic, const char *data, int, int, bool) { printf("%s|%s\n", topic, data); return 0; }
`

function program(cfg) {
  return STUBS + slot('mqtt_statics.c', cfg) + String.raw`
static void create(void)
${slot('mqtt_device_create.c', cfg)}
static void driver_cb(app_driver_param_id_t param_id, app_driver_param_val_t val)
{
    (void)val;
    switch (param_id) {
${slot('mqtt_driver_cb_cases.c', cfg)}
    default:
        break;
    }
}
/* A command from the broker; this binding's own writes are not handed back
   to its driver_cb (the bus skips the source), as on the device. */
static void mqtt_handle_data(const char *topic, int tlen, const char *data, int dlen)
{
${slot('mqtt_command_cases.c', cfg)}
}
static void cmd(const char *sub, const char *payload)
{
    char topic[96];
    snprintf(topic, sizeof(topic), "zerocode/abc123/thermo/%s", sub);
    printf("--\n");
    mqtt_handle_data(topic, (int)strlen(topic), payload, (int)strlen(payload));
}
static void set(int id, int v) { app_driver_param_val_t x = {}; if (id == P_HEAT) x.b = v; else if (id == P_MODE) x.u8 = (uint8_t)v; else x.i16 = (int16_t)v; g_param[id] = x; g_seen[id] = true; printf("--\n"); driver_cb(id, x); }
int main(int argc, char **argv)
{
    (void)argc;
    if (strcmp(argv[1], "target") == 0) {
        /* Targets from Home Assistant: out of range, off the 0.5 step, fine. */
        create();
        const char *targets[] = { "40", "21.3", "2", "22.75", "21.5" };
        for (const char *tg : targets) { cmd("temp/cmd", tg); printf("applied|%d\n", (int)g_param[P_SET].i16); }
        return 0;
    }
    if (strcmp(argv[1], "fresh") == 0) {
        /* A fresh device: only the heat output is written at boot (its driver
           writes the off it drives the pin to). Connect, then HA sets the
           mode, then the output switches on. */
        g_param[P_HEAT].b = false; g_seen[P_HEAT] = true;
        create();
        cmd("mode/cmd", "heat");
        set(P_HEAT, 1);
        return 0;
    }
    /* Restored before connect: target 20 °C, mode heat, output off. */
    g_param[P_MODE].u8 = 4; g_param[P_SET].i16 = 2000; g_param[P_HEAT].b = false;
    g_seen[P_MODE] = g_seen[P_SET] = g_seen[P_HEAT] = true;
    create();
    set(P_HEAT, 1);
    set(P_HEAT, 0);
    set(P_MODE, 0);
    set(P_HEAT, 1);
    set(P_MODE, 4);
    set(P_HEAT, 0);
    cmd("mode/cmd", "off");
    cmd("mode/cmd", "heat");
    set(P_HEAT, 1);
    cmd("mode/cmd", "off");
    return 0;
}
`
}

const CFG = { label: 'Thermostat', local_temp_param: 'P_TEMP', heat_setpoint_param: 'P_SET', system_mode_param: 'P_MODE', heating_param: 'P_HEAT' }

function run(t, cfg, scenario = 'seeded') {
  const dir = mkdtempSync(join(tmpdir(), 'zc-thermo-mqtt-'))
  t.after(() => rmSync(dir, { recursive: true, force: true }))
  writeFileSync(join(dir, 'main.cpp'), program(cfg))
  execFileSync(CXX, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable', join(dir, 'main.cpp'), '-o', join(dir, 'p')], { stdio: 'pipe' })
  const out = spawnSync(join(dir, 'p'), [scenario], { encoding: 'utf-8' })
  assert.equal(out.status, 0, out.stdout + out.stderr)
  // One array of publishes per step: [create, then each set()].
  return out.stdout.trim().split('--\n').map(chunk => chunk.trim().split('\n').filter(Boolean).map(l => l.split('|')))
}

const ACTION = 'zerocode/abc123/thermo/action'

test('thermostat mqtt: heating_param gives the climate entity an action topic that follows the heat output and the mode', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const steps = run(t, CFG)
  const disc = steps[0].find(([topic]) => topic === 'homeassistant/climate/zc_abc123_thermo/config')
  assert.ok(disc, 'discovery published')
  const json = JSON.parse(disc[1])
  assert.equal(json.action_topic, ACTION)
  assert.deepEqual(json.modes, ['off', 'heat'])
  // Seeded on connect: heat mode, output off.
  assert.deepEqual(steps[0].filter(([topic]) => topic === ACTION), [[ACTION, 'idle']])
  const action = (i) => steps[i].filter(([topic]) => topic === ACTION).map(([, v]) => v)
  assert.deepEqual(action(1), ['heating'])   // output on
  assert.deepEqual(action(2), ['idle'])      // output off, mode heat
  assert.deepEqual(action(3), ['off'])       // mode off
  assert.deepEqual(action(4), ['heating'])   // output on, whatever the mode says
  assert.deepEqual(action(5), ['heating'])   // mode heat, output still on
  assert.deepEqual(action(6), ['idle'])
  // A mode command from Home Assistant moves the action too.
  assert.deepEqual(action(7), ['off'])
  assert.deepEqual(action(8), ['idle'])
  assert.deepEqual(action(10), ['heating'])  // mode off while the output is still on
})

test('thermostat mqtt: without heating_param the discovery has no action topic and nothing publishes one', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const steps = run(t, { ...CFG, heating_param: '' })
  const disc = steps[0].find(([topic]) => topic.startsWith('homeassistant/climate/'))
  const json = JSON.parse(disc[1])
  assert.equal(json.action_topic, undefined)
  assert.equal(json.mode_command_topic, 'zerocode/abc123/thermo/mode/cmd')
  assert.ok(steps.flat().every(([topic]) => topic !== ACTION))
})

test('thermostat mqtt: on connect nothing is seeded that was never set, and the action appears as soon as the mode is known', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const steps = run(t, CFG, 'fresh')
  const topics = steps[0].map(([topic]) => topic)
  assert.ok(topics.includes('homeassistant/climate/zc_abc123_thermo/config'))
  // The output is known from boot, the mode is not: no action yet either.
  for (const sub of ['curr', 'temp/state', 'mode/state', 'action']) assert.ok(!topics.includes(`zerocode/abc123/thermo/${sub}`), sub)
  // The mode is set: published, and with the output known the action follows at once.
  assert.deepEqual(steps[1], [['zerocode/abc123/thermo/mode/state', 'heat'], [ACTION, 'idle']])
  // The output switches on.
  assert.deepEqual(steps[2], [[ACTION, 'heating']])
  // Once set, connect seeds them (the other test's scenario).
  const seeded = run(t, CFG, 'seeded')[0]
  assert.deepEqual(seeded.filter(([topic]) => !topic.startsWith('homeassistant/') && !topic.endsWith('/curr')).map(([topic, v]) => `${topic.split('/thermo/')[1] ?? topic}=${v}`),
    ['temp/state=20.00', 'mode/state=heat', 'action=idle'])
})

test('thermostat mqtt: a target command is clamped and snapped to the 0.5 step, and the state topic echoes what was applied', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const steps = run(t, CFG, 'target').slice(1)
  const STATE = 'zerocode/abc123/thermo/temp/state'
  assert.deepEqual(steps.map((s) => [s.find(([topic]) => topic === STATE)?.[1], s.find(([topic]) => topic === 'applied')?.[1]]), [
    ['35.00', '3500'],
    ['21.50', '2150'],
    ['5.00', '500'],
    ['23.00', '2300'],
    ['21.50', '2150'],
  ])
})
