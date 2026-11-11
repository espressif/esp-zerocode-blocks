// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Sensor device types with status_param: one driver status shared by several
// entities (a temperature + humidity sensor) is accepted; over MQTT a reading
// marked invalid shows as unknown ("None") in Home Assistant and the value is
// republished when the reading is valid again — also when it did not change.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))
const CXX = process.env.CXX || 'c++'
const haveCxx = spawnSync(CXX, ['--version']).status === 0

async function catalog() {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const blocks = new Map((await eng.listAllBlocks(paths)).map((b) => [b.id, b.block]))
  return { paths, blocks, cleanup: () => rmSync(dirname(tpl), { recursive: true, force: true }) }
}
const product = (instances) => ({ id: 'p', name: 'p', frameworks: ['mqtt'], instances })
const sensor = { block: 'drivers/aht21', prefix: 'AHT', cfg: { temp_param: 'APP_DRIVER_PARAM_T', humidity_param: 'APP_DRIVER_PARAM_H', status_param: 'APP_DRIVER_PARAM_OK' } }
const bus = { block: 'peripherals/i2c_bus', prefix: 'BUS', cfg: { sda_gpio: 8, scl_gpio: 9 } }

test('a shared status param is accepted; two device types on one value param still are not', async () => {
  const { blocks, cleanup } = await catalog()
  try {
    const ok = eng.validateProduct(product([bus, sensor,
      { block: 'device_types/temperature_sensor', prefix: 'TEMP', cfg: { value_param: 'APP_DRIVER_PARAM_T', status_param: 'APP_DRIVER_PARAM_OK' } },
      { block: 'device_types/humidity_sensor', prefix: 'HUM', cfg: { value_param: 'APP_DRIVER_PARAM_H', status_param: 'APP_DRIVER_PARAM_OK' } }]), blocks)
    assert.deepEqual(ok.errors.map((e) => e.message), [])
    const bad = eng.validateProduct(product([bus, sensor,
      { block: 'device_types/temperature_sensor', prefix: 'TEMP', cfg: { value_param: 'APP_DRIVER_PARAM_T' } },
      { block: 'device_types/generic_sensor', prefix: 'GEN', cfg: { value_param: 'APP_DRIVER_PARAM_T' } }]), blocks)
    assert.ok(bad.errors.some((e) => /duplicate case labels/.test(e.message)), 'a shared VALUE param is still refused')
  } finally { cleanup() }
})

test('MQTT: invalid -> "None" on every sharing entity, valid again -> each value republished', { skip: !haveCxx && 'no host C++ compiler' }, async () => {
  const slot = (b, f, prefix, cfg) => eng.__substituteForTest(readFileSync(join(ROOT, 'code_blocks/device_types', b, 'slots', f), 'utf8'), { prefix, prefix_lc: prefix.toLowerCase(), cfg })
  const t = { value_param: 'P_T', status_param: 'P_OK', label: 'T' }, h = { value_param: 'P_H', status_param: 'P_OK', label: 'H' }
  const src = String.raw`
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
typedef int esp_err_t;
#define ESP_OK 0
union app_driver_param_val_t { bool b; uint8_t u8; int16_t i16; uint16_t u16; uint32_t u32; };
enum { P_T, P_H, P_OK, P_N };
static app_driver_param_val_t g_v[P_N]; static bool g_seen[P_N];
static esp_err_t app_driver_get_param(int id, app_driver_param_val_t *v) { *v = g_v[id]; return ESP_OK; }
static bool app_driver_param_seen(int id) { return g_seen[id]; }
static std::string out;
static void zc_mqtt_publish(const char *topic, const char *data, int, int, bool) { out += std::string(topic) + "=" + data + " "; }
${slot('temperature_sensor', 'mqtt_statics.c', 'TEMP', t)}
${slot('humidity_sensor', 'mqtt_statics.c', 'HUM', h)}
static void cb(int param_id, app_driver_param_val_t val)
{
${slot('temperature_sensor', 'mqtt_driver_cb_watch.c', 'TEMP', t)}
${slot('humidity_sensor', 'mqtt_driver_cb_watch.c', 'HUM', h)}
    switch (param_id) {
${slot('temperature_sensor', 'mqtt_driver_cb_cases.c', 'TEMP', t)}
${slot('humidity_sensor', 'mqtt_driver_cb_cases.c', 'HUM', h)}
    default: break;
    }
}
static void set(int id, app_driver_param_val_t v) { g_v[id] = v; g_seen[id] = true; cb(id, v); }
int main()
{
    strcpy(s_temp_state_topic, "t"); strcpy(s_hum_humidity_topic, "h");
    app_driver_param_val_t v = {};
    v.b = true; set(P_OK, v); out.clear();
    v.i16 = 2150; set(P_T, v); v.i16 = 4800; set(P_H, v);
    printf("%s| ", out.c_str()); out.clear();
    v.b = false; set(P_OK, v);                         /* sensor lost */
    printf("%s| ", out.c_str()); out.clear();
    v.b = true; set(P_OK, v);                          /* back, values unchanged */
    printf("%s\n", out.c_str());
    return 0;
}
`
  const dir = mkdtempSync(join(tmpdir(), 'zc-av-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), src)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), 't=21.50 h=48.00 | t=None h=None | t=21.50 h=48.00')
  } finally { rmSync(dir, { recursive: true, force: true }) }
})
