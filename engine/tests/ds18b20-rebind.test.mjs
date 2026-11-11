// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/ds18b20 searches the 1-Wire bus again while no probe is bound and
// after a run of failed reads. Its slots are rendered as the generator renders
// them, compiled on the host as the C++ they are on the device over a stubbed
// bus and probe, and driven poll by poll.
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

function slots(names, cfg) {
  const ctx = { cfg, prefix: 'APP', prefix_lc: 'app' }
  return names.map((n) => substitute(readFileSync(join(ROOT, 'code_blocks/drivers/ds18b20/slots', n), 'utf-8'), ctx)).join('\n')
}

const STUBS = String.raw`
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NOT_FOUND 0x105
static const char *esp_err_to_name(esp_err_t) { return "err"; }
static const char *TAG = "test";
static int g_warns = 0;
#define ESP_LOGW(tag, fmt, ...) (g_warns++, (void)(tag), (void)snprintf(NULL, 0, fmt, ##__VA_ARGS__))
#define ESP_LOGI(tag, fmt, ...) ((void)(tag), (void)snprintf(NULL, 0, fmt, ##__VA_ARGS__))
#define ESP_LOGE ESP_LOGW

enum { PARAM_TEMP, PARAM_OK, PARAM_MAX };
typedef union { bool b; uint8_t u8; uint16_t u16; int16_t i16; uint32_t u32; } app_driver_param_val_t;
#define APP_DRIVER_SOURCE_LOCAL 0
static app_driver_param_val_t g_param[PARAM_MAX];
static int g_writes[PARAM_MAX];
static esp_err_t app_driver_set_param(int id, app_driver_param_val_t v, int) { g_param[id] = v; g_writes[id]++; return ESP_OK; }

typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
static TickType_t xTaskGetTickCount(void) { return 0; }
static void vTaskDelayUntil(TickType_t *, TickType_t) {}
typedef void (*TaskFunction_t)(void *);
static int xTaskCreate(TaskFunction_t, const char *, int, void *, int, void *) { return 1; }

/* The bus: a probe that is plugged in or not, and answers or not. */
static bool g_present = false, g_answers = true;
static int g_searches = 0, g_open = 0, g_deleted = 0;
static float g_temp = 21.5f;
typedef struct { int bus; } *onewire_bus_handle_t;
typedef struct { int it; int left; } *onewire_device_iter_handle_t;
typedef struct { int addr; } onewire_device_t;
typedef struct { int dev; } *ds18b20_device_handle_t;
typedef struct { int unused; } ds18b20_config_t;
#define DS18B20_RESOLUTION_12B 3
static int g_busobj, g_devobj;
static struct { int it; int left; } g_iter;
static onewire_bus_handle_t ow_handle(void) { return (onewire_bus_handle_t)&g_busobj; }
static esp_err_t onewire_new_device_iter(onewire_bus_handle_t, onewire_device_iter_handle_t *it)
{
    g_searches++; g_iter.left = g_present ? 1 : 0; *it = (onewire_device_iter_handle_t)&g_iter; return ESP_OK;
}
static esp_err_t onewire_device_iter_get_next(onewire_device_iter_handle_t it, onewire_device_t *)
{
    if (it->left <= 0) return ESP_ERR_NOT_FOUND;
    it->left--; return ESP_OK;
}
static esp_err_t onewire_del_device_iter(onewire_device_iter_handle_t) { return ESP_OK; }
static esp_err_t ds18b20_new_device_from_enumeration(onewire_device_t *, const ds18b20_config_t *, ds18b20_device_handle_t *h)
{
    *h = (ds18b20_device_handle_t)&g_devobj; g_open++; return ESP_OK;
}
static esp_err_t ds18b20_set_resolution(ds18b20_device_handle_t, int) { return ESP_OK; }
static esp_err_t ds18b20_del_device(ds18b20_device_handle_t) { g_deleted++; g_open--; return ESP_OK; }
static esp_err_t ds18b20_trigger_temperature_conversion(ds18b20_device_handle_t) { return (g_present && g_answers) ? ESP_OK : ESP_FAIL; }
static esp_err_t ds18b20_get_temperature(ds18b20_device_handle_t, float *t) { *t = g_temp; return ESP_OK; }
`

const MAIN = String.raw`
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void polls(int n) { for (int i = 0; i < n; i++) app_ds_poll(); }

int main(void)
{
    /* No probe at boot: one search, nothing published, status false. */
    boot();
    CHECK(g_searches == 1 && g_open == 0);
    polls(1);
    CHECK(g_writes[PARAM_TEMP] == 0 && g_param[PARAM_OK].b == false);

    /* While unbound the bus is searched every rescan (10 s = 2 polls), not every poll. */
    int s = g_searches;
    polls(10);
    CHECK(g_searches - s == 5);
    CHECK(g_writes[PARAM_TEMP] == 0);

    /* Plugged in later: found within one rescan, and it reports. */
    g_present = true;
    polls(2);
    CHECK(g_open == 1);
    CHECK(g_writes[PARAM_TEMP] >= 1 && g_param[PARAM_TEMP].i16 == 2150 && g_param[PARAM_OK].b == true);

    /* Bound: no more searches while it reads, one reading a poll. */
    s = g_searches;
    int w = g_writes[PARAM_TEMP];
    polls(5);
    CHECK(g_searches == s && g_writes[PARAM_TEMP] == w + 5);

    /* Stops answering: no temperature published, status false; after three
       failures the handle is released and the next poll searches again. */
    g_answers = false;
    w = g_writes[PARAM_TEMP];
    polls(3);
    CHECK(g_writes[PARAM_TEMP] == w && g_param[PARAM_TEMP].i16 == 2150 && g_param[PARAM_OK].b == false);
    CHECK(g_deleted == 1 && g_open == 0);
    s = g_searches;
    g_answers = true;
    g_temp = 19.0f;
    polls(1);
    CHECK(g_searches == s + 1 && g_open == 1);
    CHECK(g_param[PARAM_TEMP].i16 == 1900 && g_param[PARAM_OK].b == true);

    /* Unplugged: released after three failures, then searched every rescan,
       and nothing fake is ever published. */
    g_present = false;
    w = g_writes[PARAM_TEMP];
    polls(3);
    CHECK(g_open == 0);
    s = g_searches;
    polls(20);
    CHECK(g_searches - s == 10);
    CHECK(g_writes[PARAM_TEMP] == w && g_param[PARAM_TEMP].i16 == 1900);
    /* Quiet: a miss is logged the first time, then once a minute. */
    int warns = g_warns;
    polls(24);   /* two minutes = 12 searches */
    CHECK(g_warns - warns == 2);

    if (fails == 0) printf("ok\n");
    return fails == 0 ? 0 : 1;
}
`

const CFG = { poll_interval_ms: 5000, rescan_interval_ms: 10000, rebind_after_failures: 3, bus_handle_fn: 'ow_handle', temp_param: 'PARAM_TEMP', status_param: 'PARAM_OK' }

function program(cfg) {
  const defs = slots(['config_defines.h'], cfg)
  const statics = slots(['logic_statics.c'], cfg).replace(/^extern onewire_bus_handle_t .*\n/m, '')
  const init = slots(['logic_init.c'], cfg)
  return STUBS + defs + '\n' + statics + `\nstatic void boot(void)\n${init}\n` + MAIN
}

test('ds18b20: a probe missing at boot or lost later is searched for again and reports without a reboot', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-ds18b20-'))
  t.after(() => rmSync(dir, { recursive: true, force: true }))
  writeFileSync(join(dir, 'main.cpp'), program(CFG))
  execFileSync(CXX, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-gnu-zero-variadic-macro-arguments', join(dir, 'main.cpp'), '-o', join(dir, 'p')], { stdio: 'pipe' })
  const run = spawnSync(join(dir, 'p'), { encoding: 'utf-8' })
  assert.equal(run.status, 0, run.stdout + run.stderr)
  assert.equal(run.stdout.trim(), 'ok')
})
