// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/sntp_time day_param: the local date as days since 2000-01-01,
// never published before the clock has synced, and changing at LOCAL midnight
// (a daily high/low that keyed on an unsynced clock never rolled over).
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
const SLOTS = join(ROOT, 'code_blocks/behaviors/sntp_time/slots')
const cfg = { tz: 'IST-5:30', ntp_server: 'pool.ntp.org', valid_param: '', day_param: 'P_DAY' }
const fill = (f) => eng.__substituteForTest(readFileSync(join(SLOTS, f), 'utf8'), { prefix: 'NTP', prefix_lc: 'ntp', cfg })

test('sntp_time day_param: nothing before sync; the local date after; it changes at local midnight', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const src = String.raw`
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>
#define CONFIG_SOC_WIFI_SUPPORTED 1
#define ESP_LOGI(tag, ...) ((void)(tag))
static const char *TAG = "t";
union app_driver_param_val_t { bool b; uint16_t u16; };
#define APP_DRIVER_SOURCE_LOCAL 0
enum { P_DAY };
static std::string out; static int g_last = -1;
static void app_driver_set_param(int, app_driver_param_val_t v, int) { if ((int)v.u16 != g_last) { g_last = v.u16; out += std::to_string(v.u16) + " "; } }
static time_t g_now = 0;
extern "C" time_t time(time_t *t) { if (t) *t = g_now; return g_now; }
${fill('config_defines.h')}
${fill('logic_statics.c')}
int main()
{
    setenv("TZ", "IST-5:30", 1); tzset();
    g_now = 1000;                       /* 1970: not synced yet */
    ntp_day_tick(nullptr); out += "| ";
    g_now = 1793322180;                 /* 2026-10-30 01:03 UTC = 06:33 IST */
    ntp_sntp_synced(nullptr); out += "| ";
    ntp_day_tick(nullptr); out += "| "; /* same day: no change */
    g_now = 1793384999; ntp_day_tick(nullptr); out += "| ";   /* 2026-10-30 23:59:59 IST */
    g_now += 2; ntp_day_tick(nullptr);  /* 00:00:01 IST next day */
    printf("%s\n", out.c_str());
    return 0;
}
`
  const dir = mkdtempSync(join(tmpdir(), 'zc-day-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), src)
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '| 9799 | | | 9800')
  } finally { rmSync(dir, { recursive: true, force: true }) }
})
