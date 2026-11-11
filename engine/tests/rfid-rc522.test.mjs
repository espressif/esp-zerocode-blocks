// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/rfid_rc522: the library's state changes become a card arriving (with
// its FULL uid — a 7-byte NTAG, which hand-written readers truncated to 3) and
// a card taken away; the present param, the card event and rfid_on_card agree.
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
const BUS = join(ROOT, 'base_firmware/components/app_driver')
const SLOTS = join(ROOT, 'code_blocks/drivers/rfid_rc522/slots')

test('rfid_rc522: arrival with the full UID, removal, and nothing for other state changes', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const cfg = { i2c_port: 0, i2c_address: 40, rst_gpio: -1, poll_ms: 125, present_param: 'APP_DRIVER_PARAM_POWER', card_param: 'APP_DRIVER_PARAM_LED_PATTERN' }
  const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'R', prefix_lc: 'r', cfg })
  const prog = `#include "app_driver.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>
#define ESP_LOGI(tag, ...) ((void)(tag))
static const char *TAG = "t";
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
typedef const char *esp_event_base_t;
typedef void *rc522_driver_handle_t; typedef void *rc522_handle_t;
#define RC522_PICC_UID_SIZE_MAX 10
typedef enum { RC522_PICC_STATE_IDLE = 0, RC522_PICC_STATE_READY, RC522_PICC_STATE_ACTIVE, RC522_PICC_STATE_HALT } rc522_picc_state_t;
typedef struct { uint8_t value[RC522_PICC_UID_SIZE_MAX]; uint8_t length; } rc522_picc_uid_t;
typedef struct { rc522_picc_uid_t uid; rc522_picc_state_t state; } rc522_picc_t;
typedef struct { rc522_picc_state_t old_state; rc522_picc_t *picc; } rc522_picc_state_changed_event_t;
static esp_err_t rc522_picc_uid_to_str(const rc522_picc_uid_t *, char *b, uint8_t) { b[0] = 0; return ESP_OK; }
${fill('config_defines.h')}
${fill('logic_statics.c')}
static int g_events = 0, g_calls = 0, g_len = 0, g_present = -1; static uint8_t g_uid[10];
static void count(app_driver_param_id_t id, app_driver_param_val_t, app_driver_handle_t, void *) { if (id == APP_DRIVER_PARAM_LED_PATTERN) g_events++; }
static void on_card(const uint8_t *uid, uint8_t len, bool present, void *) { g_calls++; g_len = len; g_present = present; memcpy(g_uid, uid, len); }
static void ev(rc522_picc_state_t from, rc522_picc_state_t to) {
    rc522_picc_t p = {}; p.state = to; p.uid.length = 7; for (int i = 0; i < 7; i++) p.uid.value[i] = (uint8_t)(0x04 + i);
    rc522_picc_state_changed_event_t e = { from, &p }; r_rfid_event(nullptr, "rc522", 0, &e);
}
static int present(void) { app_driver_param_val_t v = {}; app_driver_get_param(APP_DRIVER_PARAM_POWER, &v); return v.b; }
int main() {
    app_driver_register_solution("t", count, nullptr);
    rfid_on_card(on_card, nullptr);
    ev(RC522_PICC_STATE_IDLE, RC522_PICC_STATE_READY);              /* not yet a card */
    printf("%d %d ", g_calls, g_events);
    ev(RC522_PICC_STATE_READY, RC522_PICC_STATE_ACTIVE);            /* arrived */
    printf("%d %d %d %d %02x%02x ", g_calls, g_events, present(), g_len, g_uid[0], g_uid[6]);
    ev(RC522_PICC_STATE_ACTIVE, RC522_PICC_STATE_IDLE);             /* taken away */
    printf("%d %d %d %d\\n", g_calls, g_events, present(), g_present);
    return 0;
}`
  const dir = mkdtempSync(join(tmpdir(), 'zc-rfid-'))
  try {
    writeFileSync(join(dir, 'a.cpp'), prog)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-pthread', '-DAPP_DRIVER_POLL_NO_TASK', '-I', join(BUS, 'test/shims'), '-I', join(BUS, 'test'), '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'a.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '0 0 1 1 1 7 040a 2 1 0 0')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})

test('the I2C and SPI readers both define rfid_on_card: composing both is refused before the link', async () => {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  try {
    execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
      '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
    const blocks = new Map((await eng.listAllBlocks({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') })).map((b) => [b.id, b.block]))
    const both = [{ block: 'drivers/rfid_rc522', prefix: 'A' }, { block: 'drivers/rfid_rc522_spi', prefix: 'B' }]
    assert.deepEqual(eng.duplicateCSymbols(both, blocks), [
      'drivers/rfid_rc522 (prefix=A) and drivers/rfid_rc522_spi (prefix=B) each define rfid_on_card(), so the product cannot link — compose only one of them',
    ])
    assert.deepEqual(eng.duplicateCSymbols([both[0]], blocks), [])
  } finally {
    rmSync(dirname(tpl), { recursive: true, force: true })
  }
})
