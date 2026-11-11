// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// app_mqtt.cpp: publishes are queued (the caller never waits on the network),
// and product code can add its own Home Assistant entities.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { genAppMqttCpp } from '../dist/generator.js'

function fw(name) {
  return { block: { id: `frameworks/${name}`, kind: 'framework', slots: {} }, prefix: name.toUpperCase(), cfg: {}, slots: {} }
}

for (const mix of [['mqtt'], ['matter', 'mqtt']]) {
  test(`${mix.join('+')}: esp_mqtt_client_publish is called only by the send task`, () => {
    const out = genAppMqttCpp(mix.map(fw))
    assert.equal(out.match(/esp_mqtt_client_publish\(/g).length, 1)
    assert.match(out, /static int zc_mqtt_send\([^)]*\)\n\{\n[^}]*esp_mqtt_client_publish\(/)
    assert.match(out, /xQueueSend\(s_tx_q, &m, 0\)/)
  })

  test(`${mix.join('+')}: product entity API and connect hook are defined`, () => {
    const out = genAppMqttCpp(mix.map(fw))
    for (const fn of ['app_mqtt_entity_add', 'app_mqtt_entity_set_state', 'app_mqtt_entity_on_command']) {
      assert.match(out, new RegExp(`^esp_err_t ${fn}\\(`, 'm'), fn)
    }
    assert.match(out, /__attribute__\(\(weak\)\) void app_mqtt_on_connected\(void\)/)
    const connected = out.slice(out.indexOf('case MQTT_EVENT_CONNECTED:'), out.indexOf('case MQTT_EVENT_DISCONNECTED:'))
    assert.ok(connected.indexOf('zc_entities_on_connected();') < connected.indexOf('app_mqtt_on_connected();'))
    assert.match(out, /zc_entities_dispatch\(topic, tlen, data, dlen\);/)
  })
}

// An id that is never added gets an error naming app_mqtt_entity_add, once per
// id: a 10-minute timer must not flood the log, and a silent drop hid a
// missing app_mqtt_entity_add. A state set before the add is kept (below).
test('an unknown object_id is logged once per id, naming app_mqtt_entity_add', (t) => {
  const out = genAppMqttCpp([fw('mqtt')])
  const report = out.slice(out.indexOf('static void zc_entity_report_unadded('), out.indexOf('static void zc_entities_on_connected(void)'))
  assert.match(report, /if \(!zc_unknown_first\(e->object_id\)\) return;/)
  assert.match(report, /ESP_LOGE\(TAG, "app_mqtt_entity_set_state\(\\"%s\\"\): no entity[^;]*call "\s*"app_mqtt_entity_add\(/)
  assert.match(report, /ESP_LOGE\(TAG, "app_mqtt_entity_on_command/)
  const onConn = out.slice(out.indexOf('static void zc_entities_on_connected(void)'), out.indexOf('static int zc_entities_pending('))
  assert.match(onConn, /zc_entity_report_unadded\(e\);/)

  const cxx = process.env.CXX || 'c++'
  if (spawnSync(cxx, ['--version']).status !== 0) return t.skip('no host C++ compiler')
  const fn = out.match(/#define ZC_UNKNOWN_IDS[\s\S]*?\nstatic bool zc_unknown_first\(const char \*object_id\)\n\{[\s\S]*?\n\}\n/)
  assert.ok(fn, 'zc_unknown_first in app_mqtt.cpp')
  const dir = mkdtempSync(join(tmpdir(), 'zc-mqtt-unknown-'))
  t.after(() => rmSync(dir, { recursive: true, force: true }))
  writeFileSync(join(dir, 'main.cpp'), `#include <stdio.h>
#include <string.h>
static size_t zc_test_strlcpy(char *d, const char *s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
#define strlcpy zc_test_strlcpy
${fn[0]}
int main(void)
{
    int bad = 0;
    bad += !zc_unknown_first("moisture");
    bad += zc_unknown_first("moisture");
    bad += !zc_unknown_first("tank");
    bad += zc_unknown_first("moisture");
    char id[8];
    for (int i = 0; i < 20; i++) { snprintf(id, sizeof(id), "x%d", i); zc_unknown_first(id); }
    bad += zc_unknown_first("x0");
    bad += zc_unknown_first("never_seen");  /* table full: quiet, never a crash */
    printf("%d\\n", bad);
    return bad;
}
`)
  execFileSync(cxx, ['-std=c++17', '-Wall', '-Wextra', '-Werror', join(dir, 'main.cpp'), '-o', join(dir, 'u')], { stdio: 'pipe' })
  const run = spawnSync(join(dir, 'u'), { encoding: 'utf-8' })
  assert.equal(run.status, 0, run.stdout + run.stderr)
})

// ── A state set before app_mqtt_entity_add is kept ──────────────────────
// The republish and product-entity sections of app_mqtt.cpp, compiled on the
// host over a fake send queue: a product's first sensor value arrives (the
// driver bus replays it at registration) before it adds its entities.

const ENT_STUBS = String.raw`
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t zc_test_strlcpy(char *d, const char *s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
#define strlcpy zc_test_strlcpy
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_NOT_FOUND 0x105
typedef void *SemaphoreHandle_t;
typedef int StaticSemaphore_t;
typedef int portMUX_TYPE;
typedef void *TaskHandle_t;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portMAX_DELAY 0xffffffffu
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)
#define taskENTER_CRITICAL(m) (void)(m)
#define taskEXIT_CRITICAL(m) (void)(m)
#define xSemaphoreCreateMutexStatic(b) ((SemaphoreHandle_t)(b))
#define xSemaphoreTake(h, t) ((void)(h), 1)
#define xSemaphoreGive(h) ((void)(h), 1)
static int g_errors = 0;
static char g_last_error[512];
#define ESP_LOGE(tag, fmt, ...) (g_errors++, (void)snprintf(g_last_error, sizeof(g_last_error), fmt, __VA_ARGS__))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, fmt, ...) ((void)(tag), (void)snprintf(NULL, 0, fmt, __VA_ARGS__))
static const char *TAG = "app_mqtt";
typedef void (*app_mqtt_command_cb_t)(const char *object_id, const char *payload, void *ctx);
static char s_node_id[13] = "aabbccddeeff";
static char s_status_topic[48] = "zerocode/aabbccddeeff/status";
static volatile bool s_connected = false;
static void *s_client = (void *)1;
static volatile int s_tx_pending = 0;
static bool g_on_mqtt_task = false;
static bool zc_on_mqtt_task(void) { return g_on_mqtt_task; }
struct sent_t { char topic[96]; char data[64]; };
static sent_t g_sent[64];
static int g_nsent = 0;
static int zc_mqtt_queue(bool subscribe, const char *topic, const char *data, int len, int qos, bool retain)
{
    (void)qos; (void)retain;
    if (subscribe || g_nsent >= 64) return 0;
    int dl = len > 0 ? len : (int)strlen(data);
    snprintf(g_sent[g_nsent].topic, sizeof(g_sent[0].topic), "%s", topic);
    snprintf(g_sent[g_nsent].data, sizeof(g_sent[0].data), "%.*s", dl, data);
    g_nsent++;
    return 0;
}
typedef void (*task_fn_t)(void *);
static int xTaskCreate(task_fn_t, const char *, int, void *, int, TaskHandle_t *h) { *h = (TaskHandle_t)1; return pdPASS; }
static void vTaskDelay(uint32_t) {}
`

const ENT_MAIN = String.raw`
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static int sent(const char *oid, const char *data)
{
    char topic[96];
    snprintf(topic, sizeof(topic), "zerocode/aabbccddeeff/product/%s/state", oid);
    int n = 0;
    for (int i = 0; i < g_nsent; i++) if (strcmp(g_sent[i].topic, topic) == 0 && strcmp(g_sent[i].data, data) == 0) n++;
    return n;
}
static void connect(void) { s_connected = true; g_on_mqtt_task = true; zc_entities_on_connected(); g_on_mqtt_task = false; }

int main(void)
{
    /* Before the add, while offline: kept (latest only), nothing sent, no error. */
    CHECK(app_mqtt_entity_set_state("moisture", "40") == ESP_OK);
    CHECK(app_mqtt_entity_set_state("moisture", "41") == ESP_OK);
    CHECK(g_nsent == 0 && g_errors == 0);
    CHECK(app_mqtt_entity_add("sensor", "moisture", "Soil moisture", NULL) == ESP_OK);
    CHECK(g_nsent == 0);
    /* The connect announces it with that value as its state. */
    connect();
    CHECK(sent("moisture", "41") == 1 && sent("moisture", "40") == 0);
    CHECK(g_errors == 0);

    /* Connected, set before the add: kept, then sent by the add itself. */
    CHECK(app_mqtt_entity_set_state("tank", "OFF") == ESP_OK);
    CHECK(sent("tank", "OFF") == 0);
    CHECK(g_errors == 1 && strstr(g_last_error, "\"tank\"") != NULL);  /* connected and not added */
    CHECK(app_mqtt_entity_add("binary_sensor", "tank", "Tank empty", NULL) == ESP_OK);
    CHECK(sent("tank", "OFF") == 1);
    /* An unchanged value after that is not sent again; a change is. */
    CHECK(app_mqtt_entity_set_state("tank", "OFF") == ESP_OK && sent("tank", "OFF") == 1);
    CHECK(app_mqtt_entity_set_state("tank", "ON") == ESP_OK && sent("tank", "ON") == 1);

    /* Only on schedule: a kept value is the entity's FIRST value, sent at the add. */
    app_mqtt_set_republish_only_on_schedule(true);
    CHECK(app_mqtt_set_republish_s(600) == ESP_OK);
    CHECK(app_mqtt_entity_set_state("temp", "21.5") == ESP_OK);
    CHECK(app_mqtt_entity_add("sensor", "temp", "Temperature", NULL) == ESP_OK);
    CHECK(sent("temp", "21.5") == 1);
    CHECK(app_mqtt_entity_set_state("temp", "22.0") == ESP_OK && sent("temp", "22.0") == 0);  /* held */

    /* An id never added: error once per id at connect, never again. */
    s_connected = false;
    int before = g_errors;
    CHECK(app_mqtt_entity_set_state("typo_id", "1") == ESP_OK);
    CHECK(g_errors == before);
    connect();
    CHECK(g_errors == before + 1 && strstr(g_last_error, "\"typo_id\"") != NULL);
    connect();
    CHECK(app_mqtt_entity_set_state("typo_id", "2") == ESP_OK);
    CHECK(g_errors == before + 1);
    CHECK(sent("typo_id", "1") == 0 && sent("typo_id", "2") == 0);

    /* At most 8 ids wait; the next is refused, with its error once. */
    s_connected = false;
    char id[16];
    for (int i = 0; i < 7; i++) { snprintf(id, sizeof(id), "w%d", i); CHECK(app_mqtt_entity_set_state(id, "x") == ESP_OK); }
    CHECK(app_mqtt_entity_set_state("w7", "x") == ESP_ERR_NOT_FOUND);  /* typo_id is the 8th */
    CHECK(app_mqtt_entity_set_state("w0", "y") == ESP_OK);             /* a waiting id keeps updating */
    CHECK(app_mqtt_entity_set_state("Bad-Id", "x") == ESP_ERR_INVALID_ARG);

    if (fails == 0) printf("ok\n");
    return fails == 0 ? 0 : 1;
}
`

test('a state set before app_mqtt_entity_add is kept and becomes the entity\'s first state', (t) => {
  const cxx = process.env.CXX || 'c++'
  if (spawnSync(cxx, ['--version']).status !== 0) return t.skip('no host C++ compiler')
  const out = genAppMqttCpp([fw('mqtt')])
  const start = out.indexOf('/* ── Republish (app_mqtt_set_republish_s)')
  const end = out.indexOf('/* zerocode/<node>/product/<object_id>/set -> that entity')
  assert.ok(start > 0 && end > start, 'republish + product entity sections in app_mqtt.cpp')
  const dir = mkdtempSync(join(tmpdir(), 'zc-mqtt-pending-'))
  t.after(() => rmSync(dir, { recursive: true, force: true }))
  writeFileSync(join(dir, 'main.cpp'), ENT_STUBS + out.slice(start, end) + ENT_MAIN)
  execFileSync(cxx, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', join(dir, 'main.cpp'), '-o', join(dir, 'e')], { stdio: 'pipe' })
  const run = spawnSync(join(dir, 'e'), { encoding: 'utf-8' })
  assert.equal(run.status, 0, run.stdout + run.stderr)
  assert.equal(run.stdout.trim(), 'ok')
})
