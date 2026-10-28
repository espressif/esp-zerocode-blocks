// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/ct_clamp_adc: the generated sampling code runs on the host against a
// simulated clamp (a mid-supply bias, a mains sine, ADC noise) — the bias must
// not read as current, the idle noise must read as 0 once zeroed, and a load
// must read its RMS current.
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

async function generateClamp() {
  const tpl = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const outDir = join(mkdtempSync(join(tmpdir(), 'zc-out-')), 'plug')
  const product = {
    id: 'plug', name: 'plug', frameworks: ['mqtt'],
    exclude: ['behaviors/ota_basic', 'behaviors/network_status_log', 'behaviors/uptime_log', 'behaviors/wifi_disconnect_recovery'],
    instances: [{ block: 'drivers/ct_clamp_adc', prefix: 'CLAMP', cfg: { gpio: 2, current_param: 'APP_DRIVER_PARAM_RMS_CURRENT', power_param: 'APP_DRIVER_PARAM_ACTIVE_POWER' } }],
  }
  await eng.generate({ templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware') }, { product, board: null, outDir, chip: 'esp32c6' })
  return { outDir, cleanup: () => { rmSync(dirname(tpl), { recursive: true, force: true }); rmSync(dirname(outDir), { recursive: true, force: true }) } }
}

const HARNESS = (defines, region) => String.raw`
#include "app_driver.h"
#include "esp_timer.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <string>
#include "freertos/FreeRTOS.h"
#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif
#ifndef portMAX_DELAY
#define portMAX_DELAY 0xffffffffu
#endif
#define ESP_LOGW(tag, fmt, ...) ((void)(tag))
#define ESP_LOGE(tag, fmt, ...) ((void)(tag))
#define ESP_LOGI(tag, fmt, ...) ((void)(tag))
#define ESP_LOGD(tag, fmt, ...) ((void)(tag))
esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }
static const char *TAG = "t";
${defines}
typedef void *adc_oneshot_unit_handle_t;
typedef int adc_channel_t;
typedef void *adc_cali_handle_t;
typedef void *SemaphoreHandle_t;
static SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
static int xSemaphoreTake(SemaphoreHandle_t, uint32_t) { return 1; }
static int xSemaphoreGive(SemaphoreHandle_t) { return 1; }
typedef uint32_t nvs_handle_t;
enum { NVS_READONLY, NVS_READWRITE };
#define ESP_ERR_NVS_NOT_FOUND 0x1102
static esp_err_t nvs_open(const char *, int, nvs_handle_t *) { return ESP_FAIL; }
static esp_err_t nvs_get_i32(nvs_handle_t, const char *, int32_t *) { return ESP_FAIL; }
static esp_err_t nvs_set_i32(nvs_handle_t, const char *, int32_t) { return ESP_FAIL; }
static esp_err_t nvs_erase_key(nvs_handle_t, const char *) { return ESP_FAIL; }
static esp_err_t nvs_commit(nvs_handle_t) { return ESP_FAIL; }
static void nvs_close(nvs_handle_t) {}
typedef int (*esp_console_cmd_func_t)(int, char **);
struct esp_console_cmd_t { const char *command; const char *help; esp_console_cmd_func_t func; };
static esp_console_cmd_func_t g_cmd = nullptr;
static esp_err_t esp_console_cmd_register(const esp_console_cmd_t *c) { g_cmd = c->func; return ESP_OK; }
static esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t, int, int *) { return ESP_FAIL; }

/* The simulated clamp: bias + sine (amps RMS through a 30 A/V clamp) + noise. */
static double g_amps = 0, g_noise_mv = 0;
static uint32_t g_lcg = 12345;
static esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t, adc_channel_t, int *raw)
{
    zc_test_now_us += 37;   /* ~27 kS/s, not a divisor of the mains period */
    double t = zc_test_now_us / 1e6;
    g_lcg = g_lcg * 1103515245u + 12345u;
    double noise = ((g_lcg >> 8) / double(1 << 24) * 2 - 1) * g_noise_mv * 1.7320508;  /* uniform, this RMS */
    double mv = 1650 + (g_amps / ${'30.0'}) * 1000.0 * 1.4142136 * sin(2 * M_PI * 50 * t) + noise;
    int r = (int)lround(mv * 4095.0 / 3100.0);
    *raw = r < 0 ? 0 : (r > 4095 ? 4095 : r);
    return ESP_OK;
}

adc_oneshot_unit_handle_t s_clamp_adc = (void *)1;
adc_channel_t s_clamp_adc_channel = 2;
adc_cali_handle_t s_clamp_cali = nullptr;
${region}

static uint32_t read_ma(void)
{
    zc_ct_window_t w = {};
    if (!clamp_ct_sample(&w)) return 999999;
    return clamp_ct_current_ma(&w);
}

int main(int argc, char **argv)
{
    s_clamp_ct_lock = xSemaphoreCreateMutex();
    zc_ct_add(&s_clamp_ct);
    g_noise_mv = atof(argv[1]);
    std::string out;
    g_amps = 0;   out += std::to_string(read_ma()) + " ";       /* idle, built-in zero */
    char a0[] = "ct-cal", a1[] = "zero";
    char *av[] = { a0, a1 };
    g_cmd(2, av);                                                 /* owner records the zero */
    out += std::to_string(read_ma()) + " ";                       /* idle, recorded zero */
    g_amps = 1;   out += std::to_string(read_ma()) + " ";
    g_amps = 10;  out += std::to_string(read_ma()) + " ";
    printf("%s\n", out.c_str());
    return 0;
}
`

function between(src, start, end) {
  const a = src.indexOf(start)
  const b = src.indexOf(end, a)
  assert.ok(a >= 0 && b > a, `${start} .. ${end}`)
  return src.slice(a, b)
}

test('ct_clamp_adc: bias is not current, zeroed noise reads 0, a load reads its RMS', { skip: !haveCxx && 'no host C++ compiler' }, async () => {
  const { outDir, cleanup } = await generateClamp()
  const dir = mkdtempSync(join(tmpdir(), 'zc-ct-'))
  try {
    const src = readFileSync(join(outDir, 'components/app_logic/control.cpp'), 'utf8')
    const region = between(src, 'extern adc_oneshot_unit_handle_t s_clamp_adc;', 'static void clamp_ct_task')
    const cfg = readFileSync(join(outDir, 'components/app_config/include/app_config.h'), 'utf8')
    const defines = cfg.split('\n').filter((l) => l.startsWith('#define CLAMP_CT_')).join('\n')
    writeFileSync(join(dir, 'main.cpp'), HARNESS(defines, region))
    execFileSync(CXX, ['-std=c++20', '-Wno-unused-function', '-Wno-unused-parameter', '-pthread',
      '-I', join(BUS, 'test/shims'), '-I', join(outDir, 'components/app_driver/include'), '-I', join(BUS, 'include'),
      '-o', join(dir, 'a.out'), join(dir, 'main.cpp'), join(BUS, 'app_driver_cb.cpp')], { stdio: 'pipe' })
    const run = (noise) => execFileSync(join(dir, 'a.out'), [String(noise)], { encoding: 'utf8' }).trim().split('\n').pop().split(' ').map(Number)

    const [idleBuiltIn, idleZeroed, oneAmp, tenAmp] = run(15)   /* 15 mV RMS of noise: ~450 mA if left in */
    assert.ok(idleBuiltIn < 600, `the 1.65 V bias reads as no current (${idleBuiltIn} mA is noise only)`)
    assert.equal(idleZeroed, 0, 'once zeroed, an idle clamp reads 0 mA')
    assert.ok(Math.abs(oneAmp - 1000) < 60, `1 A reads ${oneAmp} mA`)
    assert.ok(Math.abs(tenAmp - 10000) < 300, `10 A reads ${tenAmp} mA`)

    const quiet = run(0)
    assert.equal(quiet[0], 0, 'no noise and no load reads 0 mA')
  } finally {
    rmSync(dir, { recursive: true, force: true })
    cleanup()
  }
})
