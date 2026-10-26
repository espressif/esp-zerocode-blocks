// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/vibration_switch's stale_after_s and behaviors/cycle_done_detector's
// resume_after_reboot and timing. Each block's slots are rendered as the generator renders
// them, compiled on the host as the C++ they are on the device over stubbed
// ESP-IDF calls, and driven second by second.
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

function slots(block, names, cfg) {
  const ctx = { cfg, prefix: 'APP', prefix_lc: 'app' }
  return names.map((n) => substitute(readFileSync(join(ROOT, 'code_blocks', block, 'slots', n), 'utf-8'), ctx)).join('\n')
}

const STUBS = String.raw`
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERROR_CHECK(x) (void)(x)
static const char *esp_err_to_name(esp_err_t) { return "err"; }
static const char *TAG = "test";
static int g_warns = 0;
#define ESP_LOGW(tag, fmt, ...) (g_warns++, (void)(tag), (void)snprintf(NULL, 0, fmt, ##__VA_ARGS__))
#define ESP_LOGI(tag, fmt, ...) ((void)(tag), (void)snprintf(NULL, 0, fmt, ##__VA_ARGS__))
#define ESP_LOGE ESP_LOGW

enum { PARAM_ACTIVITY, PARAM_STALE, PARAM_RUNNING, PARAM_DONE, PARAM_MAX };
typedef union { bool b; uint8_t u8; uint16_t u16; int16_t i16; uint32_t u32; } app_driver_param_val_t;
#define APP_DRIVER_SOURCE_LOCAL 0
static app_driver_param_val_t g_param[PARAM_MAX];
static bool g_seen[PARAM_MAX];
static int g_writes[PARAM_MAX];
static esp_err_t app_driver_set_param(int id, app_driver_param_val_t v, int src)
{
    (void)src;
    g_param[id] = v; g_seen[id] = true; g_writes[id]++;
    return ESP_OK;
}
static esp_err_t app_driver_get_param(int id, app_driver_param_val_t *v) { *v = g_param[id]; return ESP_OK; }
static bool app_driver_param_seen(int id) { return g_seen[id]; }

typedef struct { void (*callback)(void *); void *arg; int dispatch_method; const char *name; bool skip_unhandled_events; } esp_timer_create_args_t;
typedef void *esp_timer_handle_t;
#define ESP_TIMER_TASK 0
static void (*g_timer_cb)(void *) = NULL;
static esp_err_t esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *h) { g_timer_cb = a->callback; *h = (void *)1; return ESP_OK; }
static esp_err_t esp_timer_start_periodic(esp_timer_handle_t, uint64_t) { return ESP_OK; }
static int64_t g_now_us = 0;
static int64_t esp_timer_get_time(void) { return g_now_us; }
`

// ── drivers/vibration_switch: stale_after_s ─────────────────────────────

const VIB_STUBS = String.raw`
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL_ISR(m) (void)(m)
#define portEXIT_CRITICAL_ISR(m) (void)(m)
#define portENTER_CRITICAL(m) (void)(m)
#define portEXIT_CRITICAL(m) (void)(m)
#define IRAM_ATTR
typedef int gpio_num_t;
static int g_level = 0;
static int gpio_get_level(gpio_num_t) { return g_level; }
`

const VIB_MAIN = String.raw`
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* One 1 s window, with n pulses counted in it. */
static void window(int pulses)
{
    for (int i = 0; i < pulses; i++) { g_now_us += 5000; app_vib_isr(NULL); }
    g_now_us += 1000000;
    app_vib_window_cb(NULL);
}

int main(void)
{
    /* Healthy from boot, before any pulse. */
    CHECK(g_seen[PARAM_STALE] && !g_param[PARAM_STALE].b);
    CHECK(g_warns == 0);

    /* 9 quiet seconds: not yet. The 10th: stale, logged once. */
    for (int i = 0; i < 9; i++) window(0);
    CHECK(!g_param[PARAM_STALE].b);
    window(0);
    CHECK(g_param[PARAM_STALE].b);
    CHECK(g_warns == 1);
    int writes = g_writes[PARAM_STALE];
    for (int i = 0; i < 30; i++) window(0);
    CHECK(g_writes[PARAM_STALE] == writes && g_warns == 1);

    /* The next pulse clears it; the quiet clock starts over. */
    window(3);
    CHECK(!g_param[PARAM_STALE].b);
    for (int i = 0; i < 9; i++) window(0);
    CHECK(!g_param[PARAM_STALE].b);

    /* A pulse every few seconds (a machine pausing in a cycle) never goes stale. */
    for (int i = 0; i < 100; i++) window(i % 7 == 0 ? 1 : 0);
    CHECK(!g_param[PARAM_STALE].b);

    /* Activity is published as before. */
    window(2);
    CHECK(g_param[PARAM_ACTIVITY].u16 == 120);

    if (fails == 0) printf("ok\n");
    return fails == 0 ? 0 : 1;
}
`

function vibProgram(cfg) {
  const base = { gpio: 3, active_level: 1, pull: 'none', window_ms: 1000, min_pulse_gap_us: 1000, activity_param: 'PARAM_ACTIVITY', stale_after_s: 0, ...cfg }
  const defs = slots('drivers/vibration_switch', ['config_defines.h'], base)
  const statics = slots('drivers/vibration_switch', ['driver_statics.c'], base)
  // driver_init's gpio/isr setup is not under test: only its stale seed.
  const init = slots('drivers/vibration_switch', ['driver_init.c'], base)
  const seed = (init.match(/\n(\s*app_driver_param_val_t app_vib_fresh[\s\S]*?\n    \}\n)/) || ['', ''])[1]
  return STUBS + VIB_STUBS + defs + '\n' + statics + `\nstatic struct boot_t { boot_t() {${seed}} } g_boot;\n` + VIB_MAIN
}

function compileAndRun(t, name, src) {
  const dir = mkdtempSync(join(tmpdir(), `zc-${name}-`))
  t.after(() => rmSync(dir, { recursive: true, force: true }))
  writeFileSync(join(dir, 'main.cpp'), src)
  execFileSync(CXX, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-gnu-zero-variadic-macro-arguments', join(dir, 'main.cpp'), '-o', join(dir, 'p')], { stdio: 'pipe' })
  return spawnSync(join(dir, 'p'), { encoding: 'utf-8' })
}

test('vibration_switch: stale after stale_after_s with no pulse, cleared by the next pulse', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const run = compileAndRun(t, 'vib-stale', vibProgram({ stale_after_s: 10, stale_param: 'PARAM_STALE' }))
  assert.equal(run.status, 0, run.stdout + run.stderr)
  assert.equal(run.stdout.trim(), 'ok')
})

test('vibration_switch: stale_after_s 0 (the default) and no stale_param render nothing that writes it', () => {
  const base = { gpio: 3, active_level: 1, pull: 'none', window_ms: 2000, min_pulse_gap_us: 1000, activity_param: 'PARAM_ACTIVITY', stale_after_s: 0 }
  const code = slots('drivers/vibration_switch', ['driver_statics.c', 'driver_init.c'], base)
  assert.doesNotMatch(code, /stale_param|PARAM_STALE/)
  assert.match(slots('drivers/vibration_switch', ['config_defines.h'], base), /_VIB_STALE_MS\s+\(\(uint64_t\)0 \* 1000ULL\)/)
})

// ── behaviors/cycle_done_detector: resume_after_reboot ──────────────────

const CYC_STUBS = String.raw`
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
static bool g_nvs_has = false;
static uint8_t g_nvs_val = 0;
static esp_err_t nvs_open(const char *, int, nvs_handle_t *h) { *h = 1; return ESP_OK; }
static esp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *v) { if (!g_nvs_has) return 0x1102; *v = g_nvs_val; return ESP_OK; }
static esp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t v) { g_nvs_has = true; g_nvs_val = v; return ESP_OK; }
static esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
static void nvs_close(nvs_handle_t) {}
`

function cycProgram(resume) {
  const cfg = {
    min_level: 15, margin: 10, idle_level: 0, window_seconds: 60, start_active_seconds: 20, busy_active_seconds: 5,
    min_run_minutes: 1, quiet_minutes: 2, nvs_namespace: 'app', nvs_key: 'cycle_run', resume_after_reboot: resume,
    activity_param: 'PARAM_ACTIVITY', running_param: 'PARAM_RUNNING', done_param: 'PARAM_DONE',
  }
  const defs = slots('behaviors/cycle_done_detector', ['config_defines.h'], cfg)
  const statics = slots('behaviors/cycle_done_detector', ['logic_statics.c'], cfg)
  const init = slots('behaviors/cycle_done_detector', ['logic_init.c'], cfg)
  return STUBS + CYC_STUBS + defs + '\n' + statics + `\nstatic void boot(void)\n${init}\n` + String.raw`
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void seconds(int n, uint16_t level)
{
    for (int i = 0; i < n; i++) {
        app_driver_param_val_t v = { .u16 = level };
        app_driver_set_param(PARAM_ACTIVITY, v, 0);
        g_timer_cb(NULL);
    }
}

int main(void)
{
    /* Power cut mid-cycle: NVS says running; the machine stays quiet after. */
    g_nvs_has = true;
    g_nvs_val = 1;
    boot();
    bool resumed = g_param[PARAM_RUNNING].b;
    seconds(10 * 60, 0);
    printf("%s %s %s\n", resumed ? "resumed" : "dropped", g_param[PARAM_DONE].b ? "done" : "no-done",
           g_nvs_val ? "nvs-running" : "nvs-idle");

    /* Either way, a real cycle afterwards still ends in done. */
    seconds(5 * 60, 200);
    CHECK(g_param[PARAM_RUNNING].b);
    seconds(10 * 60, 0);
    CHECK(!g_param[PARAM_RUNNING].b && g_param[PARAM_DONE].b);
    return fails == 0 ? 0 : 1;
}
`
}

test('cycle_done_detector: a cycle interrupted by a restart is dropped by default, no done', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const run = compileAndRun(t, 'cyc-drop', cycProgram(false))
  assert.equal(run.status, 0, run.stdout + run.stderr)
  assert.equal(run.stdout.trim(), 'dropped no-done nvs-idle')
})

test('cycle_done_detector: resume_after_reboot true carries it on to done', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const run = compileAndRun(t, 'cyc-resume', cycProgram(true))
  assert.equal(run.status, 0, run.stdout + run.stderr)
  assert.equal(run.stdout.trim(), 'resumed done nvs-idle')
})

test('cycle_done_detector: resume_after_reboot defaults to false', () => {
  const yml = readFileSync(join(ROOT, 'code_blocks/behaviors/cycle_done_detector/block.yml'), 'utf-8')
  assert.match(yml, /resume_after_reboot:\n\s+type: bool\n\s+default: false\n/)
})

// ── behaviors/cycle_done_detector: timing from the vibration itself ─────

function cycTimingProgram(over, body) {
  const cfg = {
    min_level: 15, margin: 10, idle_level: 0, window_seconds: 60, start_active_seconds: 20, busy_active_seconds: 5,
    min_run_minutes: 1, quiet_minutes: 2, nvs_namespace: 'app', nvs_key: 'cycle_run', resume_after_reboot: false,
    activity_param: 'PARAM_ACTIVITY', running_param: 'PARAM_RUNNING', done_param: 'PARAM_DONE', ...over,
  }
  const defs = slots('behaviors/cycle_done_detector', ['config_defines.h'], cfg)
  const statics = slots('behaviors/cycle_done_detector', ['logic_statics.c'], cfg)
  const init = slots('behaviors/cycle_done_detector', ['logic_init.c'], cfg)
  return STUBS + CYC_STUBS + defs + '\n' + statics + `\nstatic void boot(void)\n${init}\n` + String.raw`
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static int g_s = 0;  /* seconds since boot */
static void seconds(int n, uint16_t level)
{
    for (int i = 0; i < n; i++) {
        app_driver_param_val_t v = { .u16 = level };
        app_driver_set_param(PARAM_ACTIVITY, v, 0);
        g_timer_cb(NULL);
        g_s++;
    }
}
/* Seconds until done turns true (or -1 within limit). */
static int until_done(int limit)
{
    for (int i = 1; i <= limit; i++) { seconds(1, 0); if (g_param[PARAM_DONE].b) return i; }
    return -1;
}
int main(void)
{
    boot();
` + body + String.raw`
    if (fails == 0) printf("ok\n");
    return fails == 0 ? 0 : 1;
}
`
}

function runCyc(t, name, over, body) {
  const run = compileAndRun(t, name, cycTimingProgram(over, body))
  assert.equal(run.status, 0, run.stdout + run.stderr)
  assert.equal(run.stdout.trim(), 'ok')
}

test('cycle_done_detector: done arrives exactly quiet_minutes after the last active second', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  runCyc(t, 'cyc-quiet', {}, String.raw`
    seconds(10, 0);
    seconds(5 * 60, 200);
    CHECK(g_param[PARAM_RUNNING].b);
    /* The 120th quiet second after the last active one, not a window later. */
    CHECK(until_done(10 * 60) == 2 * 60);
    CHECK(!g_param[PARAM_RUNNING].b);
`)
})

test('cycle_done_detector: a lone knock in the quiet time does not move the end', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  runCyc(t, 'cyc-knock', {}, String.raw`
    seconds(5 * 60, 200);
    seconds(80, 0);
    seconds(1, 200);   /* the window holds 1 active second, busy needs 5 */
    CHECK(until_done(10 * 60) == 2 * 60 - 81);
`)
})

test('cycle_done_detector: the run is measured first to last active second', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  /* 60 vibrating seconds is a 1-minute run: it counts. */
  runCyc(t, 'cyc-run60', {}, String.raw`
    seconds(30, 0);
    seconds(60, 200);
    CHECK(until_done(10 * 60) == 2 * 60);
`)
  /* 59 is shorter than the minute: a false start, never done. */
  runCyc(t, 'cyc-run59', {}, String.raw`
    seconds(30, 0);
    seconds(59, 200);
    CHECK(g_param[PARAM_RUNNING].b);
    CHECK(until_done(10 * 60) == -1);
    CHECK(!g_param[PARAM_RUNNING].b);
`)
})

test('cycle_done_detector: a quiet time shorter than the window does not restart the run it ended', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  runCyc(t, 'cyc-short-quiet', { window_seconds: 300, quiet_minutes: 1 }, String.raw`
    seconds(5 * 60, 200);
    CHECK(until_done(10 * 60) == 60);
    seconds(10 * 60, 0);
    CHECK(!g_param[PARAM_RUNNING].b && g_param[PARAM_DONE].b);
`)
})
