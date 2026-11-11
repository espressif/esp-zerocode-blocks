// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/soil_moisture_adc: `soil-cal` never publishes a reading itself. The
// periodic poll runs on the esp_timer task and the command on the console
// task, and the param bus notifies outside its lock, so a reading published
// from the console could reach listeners after a newer poll's. The command
// asks for a poll on the esp_timer task instead, which publishes the new
// percent at once. Rendered as the generator renders it, compiled on the host
// over a simulated ADC, NVS, console and esp_timer.
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
  return names.map((n) => substitute(readFileSync(join(ROOT, 'code_blocks/drivers/soil_moisture_adc/slots', n), 'utf-8'), ctx)).join('\n')
}

const STUBS = String.raw`
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NVS_NOT_FOUND 0x1102
static const char *esp_err_to_name(esp_err_t) { return "err"; }
static const char *TAG = "test";
#define ESP_LOGI(tag, fmt, ...) ((void)(tag))
#define ESP_LOGD ESP_LOGI
#define ESP_LOGW ESP_LOGI
#define ESP_ERROR_CHECK(x) do { if ((x) != ESP_OK) printf("ESP_ERROR_CHECK\n"); } while (0)

/* The bus: which task wrote the param, and what. */
enum { PARAM_SOIL, PARAM_MAX };
typedef union { bool b; uint8_t u8; uint16_t u16; } app_driver_param_val_t;
#define APP_DRIVER_SOURCE_LOCAL 0
static bool g_on_console = false;
static int g_console_writes = 0, g_timer_writes = 0;
static uint8_t g_soil = 255;
static esp_err_t app_driver_set_param(int, app_driver_param_val_t v, int)
{
    if (g_on_console) g_console_writes++; else g_timer_writes++;
    g_soil = v.u8; return ESP_OK;
}

/* The probe. */
typedef struct { int unit; } *adc_oneshot_unit_handle_t;
typedef int adc_channel_t;
static int g_raw = 2000;
static esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t, adc_channel_t, int *out) { *out = g_raw; return ESP_OK; }
static int g_unitobj;
adc_oneshot_unit_handle_t s_app_adc = (adc_oneshot_unit_handle_t)&g_unitobj;
adc_channel_t s_app_adc_channel = 0;

/* NVS: nothing stored, writes accepted. */
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
static esp_err_t nvs_open(const char *, int mode, nvs_handle_t *h) { *h = 1; return mode == NVS_READONLY ? ESP_ERR_NVS_NOT_FOUND : ESP_OK; }
static esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *, size_t *) { return ESP_ERR_NVS_NOT_FOUND; }
static esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *, size_t) { return ESP_OK; }
static esp_err_t nvs_erase_key(nvs_handle_t, const char *) { return ESP_OK; }
static esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
static void nvs_close(nvs_handle_t) {}

/* The console. */
typedef int (*esp_console_cmd_func_t)(int, char **);
typedef struct { const char *command; const char *help; const char *hint; esp_console_cmd_func_t func; } esp_console_cmd_t;
static esp_console_cmd_func_t g_cmd = NULL;
static esp_err_t esp_console_cmd_register(const esp_console_cmd_t *c) { g_cmd = c->func; return ESP_OK; }

/* esp_timer: callbacks run only when the "timer task" runs them. */
typedef void (*esp_timer_cb_t)(void *);
typedef enum { ESP_TIMER_TASK } esp_timer_dispatch_t;
typedef struct { esp_timer_cb_t callback; void *arg; esp_timer_dispatch_t dispatch_method; const char *name; bool skip_unhandled_events; } esp_timer_create_args_t;
struct sim_timer { esp_timer_cb_t cb; bool periodic; bool armed; };
typedef sim_timer *esp_timer_handle_t;
static sim_timer g_timers[4];
static int g_ntimers = 0;
static esp_err_t esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *h) { sim_timer *t = &g_timers[g_ntimers++]; t->cb = a->callback; *h = t; return ESP_OK; }
static esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t) { t->periodic = true; t->armed = true; return ESP_OK; }
static esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t) { t->periodic = false; t->armed = true; return ESP_OK; }
static esp_err_t esp_timer_stop(esp_timer_handle_t t) { t->armed = false; return ESP_OK; }
/* Run what is due on the timer task: one-shots, then (when tick) the periodic ones. */
static void timer_task(bool tick)
{
    for (int i = 0; i < g_ntimers; i++)
        if (g_timers[i].armed && (!g_timers[i].periodic || tick)) { if (!g_timers[i].periodic) g_timers[i].armed = false; g_timers[i].cb(NULL); }
}
`

const MAIN = String.raw`
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static int console(const char *verb)
{
    char a0[] = "soil-cal", a1[16];
    snprintf(a1, sizeof a1, "%s", verb);
    char *argv[] = { a0, a1 };
    g_on_console = true;
    int r = g_cmd(2, argv);
    g_on_console = false;
    return r;
}
int main(void)
{
    boot();
    timer_task(true);
    CHECK(g_soil == zc_soil_percent(2000, 3000, 1300));

    /* Calibrated wet at this reading: the console publishes nothing itself... */
    int before = g_timer_writes;
    CHECK(console("wet") == 0);
    CHECK(g_console_writes == 0);
    CHECK(g_timer_writes == before);
    /* ...and the timer task publishes the new percent without waiting for the period. */
    timer_task(false);
    CHECK(g_timer_writes == before + 1);
    CHECK(g_soil == 100);

    /* show reads and prints, never publishes. */
    CHECK(console("show") == 0);
    timer_task(false);
    CHECK(g_console_writes == 0 && g_timer_writes == before + 1);

    /* reset: back to the built-in points, published from the timer task. */
    CHECK(console("reset") == 0);
    CHECK(g_console_writes == 0);
    timer_task(false);
    CHECK(g_soil == zc_soil_percent(2000, 3000, 1300));
    CHECK(g_console_writes == 0);

    if (fails == 0) printf("ok\n");
    return fails == 0 ? 0 : 1;
}
`

const CFG = { gpio: 2, attenuation: 3, dry_raw: 3000, wet_raw: 1300, samples: 8, poll_interval_ms: 5000, value_param: 'PARAM_SOIL', raw_param: '' }

test('soil-cal: a calibration publishes from the poll task, never from the console task', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const dir = mkdtempSync(join(tmpdir(), 'zc-soil-cal-'))
  t.after(() => rmSync(dir, { recursive: true, force: true }))
  const defs = slots(['config_defines.h'], CFG)
  const statics = slots(['logic_statics.c'], CFG).replace(/^extern .*\n/gm, '')
  const init = slots(['logic_init.c'], CFG)
  writeFileSync(join(dir, 'main.cpp'), STUBS + defs + '\n' + statics + `\nstatic void boot(void)\n${init}\n` + MAIN)
  execFileSync(CXX, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-missing-field-initializers', '-Wno-gnu-zero-variadic-macro-arguments', join(dir, 'main.cpp'), '-o', join(dir, 'p')], { stdio: 'pipe' })
  const run = spawnSync(join(dir, 'p'), { encoding: 'utf-8' })
  assert.equal(run.status, 0, run.stdout + run.stderr)
  assert.match(run.stdout.trim(), /ok$/)
})
