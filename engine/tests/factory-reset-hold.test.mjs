// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Every hold-to-reset path calls the base's app_factory_reset_hold_started()
// once the press has lasted its cue and app_factory_reset_hold_cancelled() on
// an early release, while the click, long press and reset keep working. The
// slots are rendered as the generator renders them, compiled on the host as
// the C++ they are on the device over a simulated iot_button (one device per
// pin, per-callback long presses that never fire below the DEVICE's long-press
// time, a click only for a press released before it) and esp_timer, and driven
// millisecond by millisecond.
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
#define ESP_FAIL -1
static const char *TAG = "test";
static char g_trace[256];
static void trace(const char *s) { strcat(g_trace, s); }
static void log_line(const char *fmt) { if (strstr(fmt, "long press (no handler)")) trace("L"); }
#define ESP_LOGI(tag, fmt, ...) ((void)(tag), log_line(fmt))
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI

/* The product's hooks and the base's reset. */
static esp_err_t g_reset_result = ESP_OK;
static void app_factory_reset_hold_started(void) { trace("S"); }
static void app_factory_reset_hold_cancelled(void) { trace("C"); }
static esp_err_t app_utils_factory_reset(void) { trace("R"); return g_reset_result; }

/* The param bus: a click toggles PARAM_POWER. */
enum { PARAM_POWER, PARAM_MAX };
typedef union { bool b; uint32_t u32; } app_driver_param_val_t;
#define APP_DRIVER_SOURCE_LOCAL 0
static app_driver_param_val_t g_param[PARAM_MAX];
static esp_err_t app_driver_get_param(int id, app_driver_param_val_t *v) { *v = g_param[id]; return ESP_OK; }
static esp_err_t app_driver_set_param(int id, app_driver_param_val_t v, int) { g_param[id] = v; trace("K"); return ESP_OK; }

/* Simulated time + esp_timer (one task, like ESP_TIMER_TASK). */
static int64_t g_now_ms = 0;
typedef void (*esp_timer_cb_t)(void *);
typedef enum { ESP_TIMER_TASK } esp_timer_dispatch_t;
typedef struct { esp_timer_cb_t callback; void *arg; esp_timer_dispatch_t dispatch_method; const char *name; bool skip_unhandled_events; } esp_timer_create_args_t;
struct sim_timer { esp_timer_cb_t cb; void *arg; int64_t due_ms; int64_t period_ms; bool armed; };
typedef sim_timer *esp_timer_handle_t;
static sim_timer g_timers[4];
static int g_ntimers = 0;
static esp_err_t esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *h)
{ sim_timer *t = &g_timers[g_ntimers++]; t->cb = a->callback; t->arg = a->arg; t->armed = false; *h = t; return ESP_OK; }
static esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us) { t->due_ms = g_now_ms + (int64_t)(us / 1000); t->period_ms = 0; t->armed = true; return ESP_OK; }
static esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t us) { t->period_ms = (int64_t)(us / 1000); t->due_ms = g_now_ms + t->period_ms; t->armed = true; return ESP_OK; }
static esp_err_t esp_timer_stop(esp_timer_handle_t t) { t->armed = false; return ESP_OK; }
static int64_t esp_timer_get_time(void) { return g_now_ms * 1000; }
#define ESP_ERROR_CHECK(x) do { if ((x) != ESP_OK) { printf("ESP_ERROR_CHECK\n"); } } while (0)

/* Simulated iot_button: one device per pin. */
typedef enum { BUTTON_PRESS_DOWN, BUTTON_PRESS_UP, BUTTON_SINGLE_CLICK, BUTTON_DOUBLE_CLICK, BUTTON_LONG_PRESS_START, BUTTON_EVENT_MAX } button_event_t;
#define BUTTON_SHORT_PRESS_TIME_MS 1
typedef void (*button_cb_t)(void *, void *);
typedef struct { struct { uint16_t press_time; } long_press; } button_event_args_t;
struct sim_cb { button_event_t ev; int press_time; button_cb_t cb; };
struct sim_button { int long_press_ms; sim_cb cbs[16]; int n; bool pressed; int64_t down_ms; bool reached_long; };
typedef sim_button *button_handle_t;
static sim_button g_button;
static bool g_button_made = false;
static esp_err_t iot_button_register_cb(button_handle_t b, button_event_t ev, button_event_args_t *args, button_cb_t cb, void *)
{
    int pt = (ev == BUTTON_LONG_PRESS_START) ? (args ? args->long_press.press_time : b->long_press_ms) : 0;
    b->cbs[b->n++] = { ev, pt, cb };
    return ESP_OK;
}
static esp_err_t iot_button_set_param(button_handle_t, int, void *) { return ESP_OK; }
static void fire(button_event_t ev) { for (int i = 0; i < g_button.n; i++) if (g_button.cbs[i].ev == ev) g_button.cbs[i].cb(&g_button, NULL); }
static button_handle_t app_button_get_or_create(int, int, int long_press_ms)
{
    if (!g_button_made) { g_button_made = true; g_button.long_press_ms = long_press_ms ? long_press_ms : 1500; }
    return &g_button;
}
static int g_level[64];
#define GPIO_ACTIVE 0
static void press(void) { g_button.pressed = true; g_button.down_ms = g_now_ms; g_button.reached_long = false; g_level[ACTIVE_A] = GPIO_ACTIVE; g_level[ACTIVE_B] = GPIO_ACTIVE; fire(BUTTON_PRESS_DOWN); }
static void release(void)
{
    g_button.pressed = false; g_level[ACTIVE_A] = !GPIO_ACTIVE; g_level[ACTIVE_B] = !GPIO_ACTIVE;
    fire(BUTTON_PRESS_UP);
    if (!g_button.reached_long) fire(BUTTON_SINGLE_CLICK);
}
static void advance(int ms)
{
    for (int i = 0; i < ms; i++) {
        g_now_ms++;
        if (g_button.pressed) {
            int held = (int)(g_now_ms - g_button.down_ms);
            if (held >= g_button.long_press_ms) g_button.reached_long = true;
            for (int k = 0; k < g_button.n; k++)
                if (g_button.cbs[k].ev == BUTTON_LONG_PRESS_START && g_button.cbs[k].press_time >= g_button.long_press_ms && held == g_button.cbs[k].press_time)
                    g_button.cbs[k].cb(&g_button, NULL);
        }
        for (int t = 0; t < g_ntimers; t++) {
            sim_timer *tm = &g_timers[t];
            if (tm->armed && g_now_ms >= tm->due_ms) {
                if (tm->period_ms) tm->due_ms += tm->period_ms; else tm->armed = false;
                tm->cb(tm->arg);
            }
        }
    }
}
/* For the combo block's two pins. */
typedef int gpio_num_t;
typedef struct { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; } gpio_config_t;
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_ENABLE 1
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
static esp_err_t gpio_config(const gpio_config_t *) { return ESP_OK; }
static int gpio_get_level(gpio_num_t n) { return g_level[n]; }

/* board_button's board manager. */
typedef struct { int num_buttons; button_handle_t button_handles[1]; } dev_button_handles_t;
static dev_button_handles_t g_dev;
static esp_err_t esp_board_manager_get_device_handle(const char *, void **h)
{
    g_button_made = true; g_button.long_press_ms = BOARD_LONG_PRESS_MS;
    g_dev.num_buttons = 1; g_dev.button_handles[0] = &g_button; *h = &g_dev; return ESP_OK;
}
`

const MAIN = (body) => String.raw`
static int fails = 0;
static void expect(const char *what, const char *want, int line)
{
    if (strcmp(g_trace, want) != 0) { printf("FAIL line %d (%s): got '%s', want '%s'\n", line, what, g_trace, want); fails++; }
    g_trace[0] = 0;
}
#define EXPECT(what, want) expect(what, want, __LINE__)
static void hold(int ms) { press(); advance(ms); release(); advance(1000); }

int main(void)
{
    for (int i = 0; i < 64; i++) g_level[i] = !GPIO_ACTIVE;
    boot();
    advance(200);
    g_trace[0] = 0;
${body}
    if (fails == 0) printf("ok\n");
    return fails == 0 ? 0 : 1;
}
`

function run(t, program) {
  const dir = mkdtempSync(join(tmpdir(), 'zc-fr-hold-'))
  t.after(() => rmSync(dir, { recursive: true, force: true }))
  writeFileSync(join(dir, 'main.cpp'), program)
  execFileSync(CXX, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-missing-field-initializers', '-Wno-gnu-zero-variadic-macro-arguments', join(dir, 'main.cpp'), '-o', join(dir, 'p')], { stdio: 'pipe' })
  const r = spawnSync(join(dir, 'p'), { encoding: 'utf-8' })
  assert.equal(r.status, 0, r.stdout + r.stderr)
  assert.equal(r.stdout.trim(), 'ok')
}

const skip = !haveCxx && 'no host C++ compiler'

test('board_button: feedback from the cue to the reset or the release; click and long press unchanged', { skip }, (t) => {
  const cfg = { device: 'boot_button', double_click_ms: 0, long_press_ms: 2000, factory_reset_ms: 5000, reset_cue_ms: 0, target_param: 'PARAM_POWER' }
  const pre = '#define ACTIVE_A 0\n#define ACTIVE_B 0\n#define BOARD_LONG_PRESS_MS 1500\n'
  const body = String.raw`
    hold(100);   EXPECT("a click toggles, no feedback", "K");
    hold(1800);  EXPECT("past the device long press, before the cue: nothing", "");
    hold(3000);  EXPECT("cue (= long press 2 s) then release: long press, started, cancelled", "LSC");
    hold(6000);  EXPECT("held to the reset: started then reset, no cancel", "LSR");
    g_reset_result = ESP_FAIL;
    hold(6000);  EXPECT("a failed erase ends the feedback once", "LSRC");
    g_reset_result = ESP_OK;
    hold(100);   EXPECT("the click still toggles afterwards", "K");
`
  const statics = STUBS.replace('#define GPIO_ACTIVE', pre + '#define GPIO_ACTIVE')
  run(t, statics + slots('drivers/board_button', ['logic_statics.c'], cfg) + `\nstatic void boot(void)\n${slots('drivers/board_button', ['logic_init.c'], cfg)}\n` + MAIN(body))
})

test('board_button: an explicit reset_cue_ms before the long press', { skip }, (t) => {
  const cfg = { device: 'boot_button', double_click_ms: 0, long_press_ms: 2000, factory_reset_ms: 5000, reset_cue_ms: 1600, target_param: 'PARAM_POWER' }
  const pre = '#define ACTIVE_A 0\n#define ACTIVE_B 0\n#define BOARD_LONG_PRESS_MS 1500\n'
  const body = String.raw`
    hold(1550);  EXPECT("before the cue: nothing", "");
    hold(1700);  EXPECT("past the cue, released: started, cancelled", "SC");
`
  run(t, STUBS.replace('#define GPIO_ACTIVE', pre + '#define GPIO_ACTIVE') + slots('drivers/board_button', ['logic_statics.c'], cfg) + `\nstatic void boot(void)\n${slots('drivers/board_button', ['logic_init.c'], cfg)}\n` + MAIN(body))
})

for (const [label, ownerFirst] of [['a control button created the device', true], ['it created the device itself (long press = the hold)', false]]) {
  test(`factory_reset_long_press, ${label}: started at the cue though the device's long press is later`, { skip }, (t) => {
    const cfg = { gpio: 9, active_level: 0, hold_time_ms: 5000, reset_cue_ms: 1500 }
    const pre = '#define ACTIVE_A 9\n#define ACTIVE_B 9\n#define BOARD_LONG_PRESS_MS 1500\n'
    const clickOwner = ownerFirst
      ? 'static void click_cb(void *, void *) { app_driver_param_val_t v; app_driver_get_param(PARAM_POWER, &v); v.b = !v.b; app_driver_set_param(PARAM_POWER, v, 0); }\nstatic void control_button(void) { iot_button_register_cb(app_button_get_or_create(9, 0, 0), BUTTON_SINGLE_CLICK, NULL, click_cb, NULL); }\n'
      : 'static void control_button(void) {}\n'
    // A press shorter than the device's long press is a click (unchanged); with
    // the device created by this block that is every press below the hold.
    const body = ownerFirst ? String.raw`
    hold(100);   EXPECT("a click toggles, no feedback", "K");
    hold(1400);  EXPECT("before the cue: a click, no feedback", "K");
    hold(3000);  EXPECT("past the cue, released: started, cancelled, no click", "SC");
    hold(6000);  EXPECT("held to the reset", "SR");
    g_reset_result = ESP_FAIL;
    hold(6000);  EXPECT("a failed erase ends the feedback once", "SRC");
` : String.raw`
    hold(1400);  EXPECT("before the cue: nothing", "");
    hold(3000);  EXPECT("past the cue, released: started, cancelled", "SC");
    hold(6000);  EXPECT("held to the reset", "SR");
`
    const program = STUBS.replace('#define GPIO_ACTIVE', pre + '#define GPIO_ACTIVE') + clickOwner
      + slots('behaviors/factory_reset_long_press', ['config_defines.h', 'logic_statics.c'], cfg)
      + `\nstatic void fr_init(void)\n${slots('behaviors/factory_reset_long_press', ['logic_init.c'], cfg)}\nstatic void boot(void) { control_button(); fr_init(); }\n` + MAIN(body)
    run(t, program)
  })
}

test('factory_reset_button_combo: started once both are held past the cue, cancelled when either lets go', { skip }, (t) => {
  const cfg = { gpio_a: 4, gpio_b: 5, active_level: 0, hold_time_ms: 5000, reset_cue_ms: 1000 }
  const pre = '#define ACTIVE_A 4\n#define ACTIVE_B 5\n#define BOARD_LONG_PRESS_MS 1500\n'
  const body = String.raw`
    hold(800);   EXPECT("before the cue: nothing", "");
    hold(2000);  EXPECT("past the cue, released: started, cancelled", "SC");
    press(); g_level[5] = !GPIO_ACTIVE; advance(3000); release(); advance(500);
                 EXPECT("one button alone: nothing", "");
    hold(6000);  EXPECT("held to the reset: started, reset once", "SR");
    g_reset_result = ESP_FAIL;
    hold(6000);  EXPECT("a failed erase ends the feedback once, no retry while held", "SRC");
`
  const program = STUBS.replace('#define GPIO_ACTIVE', pre + '#define GPIO_ACTIVE')
    + slots('behaviors/factory_reset_button_combo', ['config_defines.h', 'logic_statics.c'], cfg)
    + `\nstatic void boot(void)\n${slots('behaviors/factory_reset_button_combo', ['logic_init.c'], cfg)}\n` + MAIN(body)
  run(t, program)
})
