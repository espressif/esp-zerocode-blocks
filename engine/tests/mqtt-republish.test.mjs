// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// frameworks/mqtt republishes every retained state on a schedule
// (app_mqtt_set_republish_s, composed as behaviors/mqtt_republish). The
// republish section of app_mqtt.cpp is compiled here, on the host, as the C++
// it is on the device, over stubbed FreeRTOS and a fake send queue, and its
// own task loop is run until a stubbed vTaskDelay stops it.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parse } from 'yaml'
import { genAppMqttCpp, missingFrameworks } from '../dist/index.js'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const CXX = process.env.CXX || 'c++'
const haveCxx = spawnSync(CXX, ['--version']).status === 0

function fw(name) {
  return { block: { id: `frameworks/${name}`, kind: 'framework', slots: {} }, prefix: name.toUpperCase(), cfg: {}, slots: {} }
}

const STUBS = String.raw`
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NO_MEM 0x101
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
static int g_logs = 0;
#define ESP_LOGW(tag, ...) (g_logs++)
#define ESP_LOGI(tag, fmt, ...) ((void)(tag), (void)snprintf(NULL, 0, fmt, __VA_ARGS__))
static const char *TAG = "app_mqtt";

static char s_status_topic[48] = "zerocode/aabbccddeeff/status";
static volatile bool s_connected = true;
static void *s_client = (void *)1;
/* Whether the caller is the MQTT client's own task (connect, a command). */
static bool g_on_mqtt_task = false;
static bool zc_on_mqtt_task(void) { return g_on_mqtt_task; }

/* The send queue: what was queued, in order. */
struct sent_t { char topic[96]; char data[64]; bool retain; };
static sent_t g_sent[64];
static int g_nsent = 0;
static int zc_mqtt_queue(bool subscribe, const char *topic, const char *data, int len, int qos, bool retain)
{
    (void)subscribe; (void)qos;
    if (g_nsent >= 64) return -1;
    int dl = len > 0 ? len : (int)strlen(data);
    snprintf(g_sent[g_nsent].topic, sizeof(g_sent[0].topic), "%s", topic);
    snprintf(g_sent[g_nsent].data, sizeof(g_sent[0].data), "%.*s", dl, data);
    g_sent[g_nsent++].retain = retain;
    return 0;
}

typedef void (*task_fn_t)(void *);
static task_fn_t g_task = NULL;
static int xTaskCreate(task_fn_t fn, const char *name, int stack, void *arg, int prio, TaskHandle_t *h)
{
    (void)name; (void)stack; (void)arg; (void)prio;
    g_task = fn;
    *h = (TaskHandle_t)fn;
    return pdPASS;
}
/* Each call is one second; the run ends after g_delay_limit of them. */
struct stop_t {};
static int g_delays = 0, g_delay_limit = 0;
static void vTaskDelay(uint32_t ticks) { (void)ticks; if (++g_delays > g_delay_limit) throw stop_t(); }
static void run_task_for(int seconds) { g_delays = 0; g_delay_limit = seconds; try { g_task(NULL); } catch (stop_t &) {} }
`

const MAIN = String.raw`
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static int count_sent(const char *topic, const char *data)
{
    int n = 0;
    for (int i = 0; i < g_nsent; i++) if (strcmp(g_sent[i].topic, topic) == 0 && strcmp(g_sent[i].data, data) == 0 && g_sent[i].retain) n++;
    return n;
}

int main(void)
{
    /* Off by default: nothing is recorded. */
    zc_mqtt_publish("zerocode/n/soil/moisture", "40", 0, 1, true);
    CHECK(s_retained.n == 0);
    CHECK(g_nsent == 1);

    CHECK(app_mqtt_set_republish_s(5) == ESP_OK);
    CHECK(g_task != NULL);

    /* State is recorded; discovery, status and non-retained messages are not. */
    zc_mqtt_publish("homeassistant/sensor/zc_n_soil/config", "{}", 0, 1, true);
    zc_mqtt_publish(s_status_topic, "online", 0, 1, true);
    zc_mqtt_publish("zerocode/n/btn/trigger", "press", 0, 1, false);
    zc_mqtt_publish("zerocode/n/soil/moisture", "41", 0, 1, true);
    zc_mqtt_publish("zerocode/n/product/tank/state", "OFF", 0, 1, true);
    zc_mqtt_publish("zerocode/n/soil/moisture", "42", 0, 1, true);
    CHECK(s_retained.n == 2);

    /* Four seconds: nothing yet. The fifth: each state once, its latest value. */
    g_nsent = 0;
    run_task_for(4);
    CHECK(g_nsent == 0);
    run_task_for(5);
    CHECK(g_nsent == 2);
    CHECK(count_sent("zerocode/n/soil/moisture", "42") == 1);
    CHECK(count_sent("zerocode/n/product/tank/state", "OFF") == 1);
    CHECK(count_sent("zerocode/n/soil/moisture", "41") == 0);

    /* The same unchanged value again on the next round. */
    g_nsent = 0;
    run_task_for(5);
    CHECK(count_sent("zerocode/n/soil/moisture", "42") == 1);

    /* Not while disconnected. */
    s_connected = false;
    g_nsent = 0;
    run_task_for(5);
    CHECK(g_nsent == 0);
    s_connected = true;

    /* An empty retained message clears the topic: nothing left to re-send. */
    zc_mqtt_publish("zerocode/n/product/tank/state", "", 0, 1, true);
    CHECK(s_retained.n == 1);

    /* Pure table: full, too long, replace keeps one entry per topic. */
    zc_retained_set_t t;
    memset(&t, 0, sizeof(t));
    char topic[32];
    for (int i = 0; i < APP_MQTT_MAX_RETAINED; i++) {
        snprintf(topic, sizeof(topic), "t/%d", i);
        CHECK(zc_retained_put(&t, topic, "v", 1, 1) == ZC_RETAINED_STORED);
    }
    CHECK(zc_retained_put(&t, "t/new", "v", 1, 1) == ZC_RETAINED_FULL);
    CHECK(zc_retained_put(&t, "t/3", "w", 1, 0) == ZC_RETAINED_STORED);
    CHECK(t.n == APP_MQTT_MAX_RETAINED);
    static char big[ZC_RETAINED_MAX_LEN + 2];
    memset(big, 'x', sizeof(big) - 1);
    CHECK(zc_retained_put(&t, "t/0", big, ZC_RETAINED_MAX_LEN + 1, 1) == ZC_RETAINED_TOO_LONG);
    zc_retained_clear(&t);
    CHECK(t.n == 0);

    /* Off again: the table is emptied and the task sends nothing. */
    CHECK(app_mqtt_set_republish_s(0) == ESP_OK);
    CHECK(s_retained.n == 0);
    g_nsent = 0;
    run_task_for(20);
    CHECK(g_nsent == 0);

    /* Only on schedule: a change waits for the schedule. */
    app_mqtt_set_republish_only_on_schedule(true);
    CHECK(app_mqtt_set_republish_s(5) == ESP_OK);
    g_nsent = 0;
    zc_mqtt_publish("zerocode/n/soil/moisture", "50", 0, 1, true);
    CHECK(count_sent("zerocode/n/soil/moisture", "50") == 1); /* the first value goes at once */
    zc_mqtt_publish("zerocode/n/soil/moisture", "51", 0, 1, true);
    zc_mqtt_publish("zerocode/n/soil/moisture", "52", 0, 1, true);
    CHECK(g_nsent == 1);
    /* Discovery, availability and events are never held. */
    zc_mqtt_publish("homeassistant/sensor/zc_n_soil/config", "{}", 0, 1, true);
    zc_mqtt_publish(s_status_topic, "online", 0, 1, true);
    zc_mqtt_publish("zerocode/n/btn/trigger", "press", 0, 1, false);
    CHECK(g_nsent == 4);
    g_nsent = 0;
    run_task_for(4);
    CHECK(g_nsent == 0);
    run_task_for(5);
    CHECK(g_nsent == 1);
    CHECK(count_sent("zerocode/n/soil/moisture", "52") == 1);
    CHECK(count_sent("zerocode/n/soil/moisture", "51") == 0);

    /* A (re)connect announces from the client's own task: sent at once. */
    g_nsent = 0;
    g_on_mqtt_task = true;
    zc_mqtt_publish("zerocode/n/soil/moisture", "53", 0, 1, true);
    g_on_mqtt_task = false;
    CHECK(count_sent("zerocode/n/soil/moisture", "53") == 1);

    /* A first value while offline is not "sent": the next one still goes. */
    s_connected = false;
    zc_mqtt_publish("zerocode/n/product/tank/state", "ON", 0, 1, true);
    s_connected = true;
    g_nsent = 0;
    zc_mqtt_publish("zerocode/n/product/tank/state", "OFF", 0, 1, true);
    CHECK(count_sent("zerocode/n/product/tank/state", "OFF") == 1);
    zc_mqtt_publish("zerocode/n/product/tank/state", "ON", 0, 1, true);
    CHECK(g_nsent == 1);

    /* Without a schedule nothing is held. */
    CHECK(app_mqtt_set_republish_s(0) == ESP_OK);
    g_nsent = 0;
    zc_mqtt_publish("zerocode/n/soil/moisture", "54", 0, 1, true);
    zc_mqtt_publish("zerocode/n/soil/moisture", "55", 0, 1, true);
    CHECK(g_nsent == 2);

    if (fails == 0) printf("ok\n");
    return fails == 0 ? 0 : 1;
}
`

test('republish re-sends the latest retained state on schedule, and only state; only_on_schedule holds changes for it', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const out = genAppMqttCpp([fw('mqtt')])
  const start = out.indexOf('/* ── Republish (app_mqtt_set_republish_s)')
  const end = out.indexOf('static int zc_mqtt_subscribe(')
  assert.ok(start > 0 && end > start, 'republish section in app_mqtt.cpp')
  const dir = mkdtempSync(join(tmpdir(), 'zc-mqtt-republish-'))
  t.after(() => rmSync(dir, { recursive: true, force: true }))
  writeFileSync(join(dir, 'main.cpp'), STUBS + out.slice(start, end) + MAIN)
  execFileSync(CXX, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', join(dir, 'main.cpp'), '-o', join(dir, 'r')], { stdio: 'pipe' })
  const run = spawnSync(join(dir, 'r'), { encoding: 'utf-8' })
  assert.equal(run.status, 0, run.stdout + run.stderr)
  assert.equal(run.stdout.trim(), 'ok')
})

test('the republish timer never runs on the esp_timer task', () => {
  const out = genAppMqttCpp([fw('mqtt')])
  const section = out.slice(out.indexOf('/* ── Republish'), out.indexOf('static int zc_mqtt_subscribe('))
  assert.doesNotMatch(section, /esp_timer/)
  assert.match(section, /xTaskCreate\(zc_republish_task,/)
})

test('behaviors/mqtt_republish needs the mqtt framework', () => {
  const block = parse(readFileSync(join(ROOT, 'code_blocks/behaviors/mqtt_republish/block.yml'), 'utf-8'))
  assert.deepEqual(block.requires_frameworks, ['mqtt'])
  const blocks = new Map([['behaviors/mqtt_republish', { id: 'behaviors/mqtt_republish', kind: 'behavior', ...block }]])
  const instances = [{ block: 'behaviors/mqtt_republish', prefix: 'REPORT' }]
  const without = missingFrameworks(instances, blocks, ['matter'])
  assert.equal(without.length, 1)
  assert.match(without[0], /behaviors\/mqtt_republish works only with the mqtt framework/)
  assert.deepEqual(missingFrameworks(instances, blocks, ['matter', 'mqtt']), [])
})

test('mqtt_republish renders only_on_schedule as a plain C condition', () => {
  const slot = readFileSync(join(ROOT, 'code_blocks/behaviors/mqtt_republish/slots/mqtt_node_init.c'), 'utf-8')
  assert.match(slot, /if \(\{\{cfg\.only_on_schedule\}\}\) \{\s*app_mqtt_set_republish_only_on_schedule\(true\);/)
  const block = parse(readFileSync(join(ROOT, 'code_blocks/behaviors/mqtt_republish/block.yml'), 'utf-8'))
  assert.equal(block.params.only_on_schedule.type, 'bool')
  assert.equal(block.params.only_on_schedule.default, false)
})
