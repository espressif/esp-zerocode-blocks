// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/camera_motion_dvp: a camera that is not ready at boot is retried
// with a growing pause until it answers, instead of disabling motion for good.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const CXX = process.env.CXX || 'c++'
const haveCxx = spawnSync(CXX, ['--version']).status === 0

test('camera_motion_dvp: init is retried with a growing pause until the camera answers', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const src = readFileSync(join(ROOT, 'code_blocks/drivers/camera_motion_dvp/slots/vision_statics.c'), 'utf8')
  const a = src.indexOf('static camera_config_t s_{{prefix_lc}}_cam_cfg;')
  const b = src.indexOf('static void {{prefix_lc}}_motion_task')
  assert.ok(a >= 0 && b > a, 'the retry helper is present')
  const helper = src.slice(a, b).replaceAll('{{prefix_lc}}', 'cam')
  const dir = mkdtempSync(join(tmpdir(), 'zc-cam-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), String.raw`
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_LOGW(tag, ...) ((void)(tag))
static const char *TAG = "t";
static const char *esp_err_to_name(esp_err_t) { return "e"; }
struct camera_config_t { int x; };
static int g_tries = 0;
static int g_answers_at = 3;
static esp_err_t esp_camera_init(const camera_config_t *) { return ++g_tries >= g_answers_at ? ESP_OK : ESP_FAIL; }
static std::string g_pauses;
#define pdMS_TO_TICKS(x) (x)
static void vTaskDelay(uint32_t ms) { g_pauses += std::to_string(ms / 1000) + " "; }
${helper}
int main(int argc, char **argv) { if (argc > 1) g_answers_at = atoi(argv[1]); cam_camera_until_up(); printf("%d %s\n", g_tries, g_pauses.c_str()); return 0; }
`)
    execFileSync(CXX, ['-std=c++17', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    assert.equal(execFileSync(join(dir, 'a.out'), { encoding: 'utf8' }).trim(), '3 5 10')
    // the pause grows to 60 s and stays there, never past it
    assert.equal(execFileSync(join(dir, 'a.out'), ['9'], { encoding: 'utf8' }).trim(), '9 5 10 20 40 60 60 60 60')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
