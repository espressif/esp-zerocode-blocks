// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/wifi_disconnect_recovery: a reconnect that lands while the outage
// check runs must not read as an outage as long as the uptime — the check read
// the timestamp twice, and a clear between the reads rebooted a device that
// had just reconnected.
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/wifi_disconnect_recovery/slots')
const cfg = { reboot_after_seconds: 300 }
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'WR', prefix_lc: 'wr', cfg })

test('wifi_disconnect_recovery: a reconnect during the check never reboots', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const harness = String.raw`
#include <cstdio>
#include <cstdint>
#define CONFIG_SOC_WIFI_SUPPORTED 1
#define ESP_LOGW(tag, ...) ((void)(tag))
static const char *TAG = "t";
typedef const char *esp_event_base_t;
static const char *WIFI_EVENT = "WIFI";
enum { WIFI_EVENT_STA_CONNECTED = 4, WIFI_EVENT_STA_DISCONNECTED = 5 };
struct portMUX_TYPE { int x; };
#define portMUX_INITIALIZER_UNLOCKED {0}
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
typedef void *esp_timer_handle_t;
static int g_reboots = 0;
static void esp_restart(void) { g_reboots++; }
static int64_t g_now = 1000LL * 1000000LL;            /* 1000 s of uptime */
static void (*g_on_clock)(void) = nullptr;
static int64_t esp_timer_get_time(void) { if (g_on_clock) { auto f = g_on_clock; g_on_clock = nullptr; f(); } return g_now; }
${fill('config_defines.h')}
${fill('logic_statics.c')}
static void reconnect(void) { wr_wifi_event_cb(nullptr, WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, nullptr); }
int main()
{
    g_now = 990LL * 1000000LL;
    wr_wifi_event_cb(nullptr, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, nullptr);   /* down at 990 s */
    g_now = 1000LL * 1000000LL;
    g_on_clock = reconnect;                    /* the reconnect lands as the check reads the clock */
    wr_recov_check(nullptr);
    printf("%d\n", g_reboots);
    return 0;
}
`
  const dir = mkdtempSync(join(tmpdir(), 'zc-wr-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), harness)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-Wno-unused-variable', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '0')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
