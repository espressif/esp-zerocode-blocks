// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// peripherals/{onewire_bus,pcnt_unit,uart_bus}: a resource that cannot be had
// (no free RMT channel / PCNT unit, a UART port the console already owns)
// leaves the peripheral unusable and says so — it never aborts the boot, and
// a UART someone else installed is never reconfigured.
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
const slot = (b, f, prefix, cfg) => eng.__substituteForTest(readFileSync(join(ROOT, 'code_blocks/peripherals', b, 'slots', f), 'utf8'), { prefix, prefix_lc: prefix.toLowerCase(), cfg })

const HARNESS = (body) => String.raw`
#include <cstdio>
#include <cstdlib>
#include <cstdint>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERROR_CHECK(x) do { if ((x) != ESP_OK) { printf("ABORT\n"); exit(1); } } while (0)
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
static const char *TAG = "t";
static const char *esp_err_to_name(esp_err_t) { return "e"; }
#define SOC_PCNT_SUPPORTED 1
/* 1-Wire: no RMT channel left. */
struct onewire_bus_config_t { int bus_gpio_num; };
struct onewire_bus_rmt_config_t { int max_rx_bytes; };
typedef void *onewire_bus_handle_t;
static esp_err_t onewire_new_bus_rmt(const onewire_bus_config_t *, const onewire_bus_rmt_config_t *, onewire_bus_handle_t *) { return ESP_ERR_NOT_FOUND; }
/* PCNT: no unit left. */
struct pcnt_unit_config_t { int high_limit, low_limit; struct { int accum_count; } flags; };
typedef void *pcnt_unit_handle_t;
static esp_err_t pcnt_new_unit(const pcnt_unit_config_t *, pcnt_unit_handle_t *) { return ESP_ERR_NOT_FOUND; }
static esp_err_t pcnt_unit_add_watch_point(pcnt_unit_handle_t, int) { return ESP_OK; }
static esp_err_t pcnt_del_unit(pcnt_unit_handle_t) { return ESP_OK; }
/* UART: port 0 already installed (the console). */
typedef int uart_port_t;
enum { UART_DATA_8_BITS, UART_PARITY_DISABLE = 0, UART_STOP_BITS_1 = 1, UART_HW_FLOWCTRL_DISABLE = 0, UART_SCLK_DEFAULT = 0 };
#define UART_PIN_NO_CHANGE -1
struct uart_config_t { int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk; };
static int g_uart_touched = 0;
static bool uart_is_driver_installed(uart_port_t p) { return p == 0; }
static esp_err_t uart_param_config(uart_port_t, const uart_config_t *) { g_uart_touched++; return ESP_OK; }
static esp_err_t uart_set_pin(uart_port_t, int, int, int, int) { g_uart_touched++; return ESP_OK; }
static esp_err_t uart_driver_install(uart_port_t, int, int, int, void *, int) { g_uart_touched++; return ESP_OK; }
${body}
`

test('peripherals degrade instead of aborting, and leave an owned UART alone', { skip: !haveCxx && 'no host C++ compiler' }, () => {
  const ow = { gpio: 4 }, pc = { high_limit: 32767, low_limit: -32768 }, ua = { port: 0, tx_gpio: 5, rx_gpio: 6, baud: 115200, rx_buf_size: 1024, tx_buf_size: 0 }
  const body = [
    slot('onewire_bus', 'config_defines.h', 'OW', ow), slot('onewire_bus', 'driver_statics.c', 'OW', ow),
    slot('pcnt_unit', 'driver_statics.c', 'PC', pc),
    slot('uart_bus', 'config_defines.h', 'U', ua),
    'int main() {',
    slot('onewire_bus', 'driver_init.c', 'OW', ow),
    slot('pcnt_unit', 'driver_init.c', 'PC', pc),
    slot('uart_bus', 'driver_init.c', 'U', ua),
    '  printf("%d %d %d\\n", ow_onewire_handle() == NULL, pc_pcnt_handle() == NULL, g_uart_touched); return 0; }',
  ].join('\n')
  const dir = mkdtempSync(join(tmpdir(), 'zc-per-'))
  try {
    writeFileSync(join(dir, 'main.cpp'), HARNESS(body))
    execFileSync(CXX, ['-std=c++17', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.cpp')], { stdio: 'pipe' })
    const r = spawnSync(join(dir, 'a.out'), { encoding: 'utf8' })
    assert.equal(r.stdout.trim(), '1 1 0', 'no abort, NULL handles, the console UART untouched')
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})
