// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// drivers/encoder on a chip without PCNT decodes quadrature with a Gray-code
// state machine on any-edge interrupts of A and B. Compiled on the host with
// the slot as written: every detent of a clean spin is counted, contact bounce
// cancels out, and the first edge after boot is not lost.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const SLOT = join(ROOT, 'code_blocks/drivers/encoder/slots/logic_statics.c')

function fallbackSource() {
  const src = readFileSync(SLOT, 'utf8')
  const body = src.slice(src.indexOf('#else /* !SOC_PCNT_SUPPORTED'), src.indexOf('#endif /* SOC_PCNT_SUPPORTED */'))
  const head = src.slice(0, src.indexOf('#if SOC_PCNT_SUPPORTED'))
  return (head + body.replace(/^#else[^\n]*\n/, ''))
    .replaceAll('{{prefix_lc}}', 'enc').replaceAll('{{prefix}}', 'ENC').replaceAll('{{cfg.position_param}}', '0')
}

const HARNESS = (seq) => `
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#define IRAM_ATTR
#define DRAM_ATTR
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL_ISR(m) (void)(m)
#define portEXIT_CRITICAL_ISR(m) (void)(m)
#define portENTER_CRITICAL(m) (void)(m)
#define portEXIT_CRITICAL(m) (void)(m)
typedef int gpio_num_t;
#define ENC_ENC_A_GPIO 1
#define ENC_ENC_B_GPIO 2
#define ENC_ENC_PER_DETENT 4
static int g_a = 1, g_b = 1;
static int gpio_get_level(gpio_num_t n) { return n == 1 ? g_a : g_b; }
typedef union { uint8_t u8; } app_driver_param_val_t;
#define APP_DRIVER_SOURCE_LOCAL 0
static void app_driver_set_param(int id, app_driver_param_val_t v, int s) { (void)id; (void)v; (void)s; }
#define __attribute__(x)
${fallbackSource()}
#undef __attribute__
int main(void) {
  static const char seq[] = "${seq}";
  s_enc_state = 3; /* resting at A=1 B=1, as init reads it */
  for (const char *p = seq; *p; p += 2) {
    g_a = p[0] - '0'; g_b = p[1] - '0';
    enc_ab_isr(0);
    if (p[2] == '|') { enc_poll_cb(0); p++; }
  }
  enc_poll_cb(0);
  printf("%d\\n", (int)s_enc_pos);
  return 0;
}
`
// enc_on_step is the weak default: it moves enc_pos, which the harness reads.

function run(seq) {
  const dir = mkdtempSync(join(tmpdir(), 'zc-enc-'))
  try {
    writeFileSync(join(dir, 'main.c'), HARNESS(seq))
    execFileSync('cc', ['-std=c11', '-Wall', '-Werror', '-Wno-unused-function', '-o', join(dir, 'a.out'), join(dir, 'main.c')], { stdio: 'pipe' })
    return Number(execFileSync(join(dir, 'a.out')).toString().trim())
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
}

// One detent clockwise from rest (11): A leads — 01, 00, 10, 11.
const CW = '01001011'
const CCW = '10000111'

test('every detent of a clean spin is counted, in both directions', () => {
  assert.equal(run(CW), 1)
  assert.equal(run(CW + CW + CW), 3)
  assert.equal(run(CCW + CCW), 254) // u8 position wraps
})

test('contact bounce on one channel cancels out', () => {
  // A chatters at the first transition: 01 11 01 11 01, then the rest.
  assert.equal(run('0111011101' + '001011'), 1)
})

test('a detent split across two polls is carried, not dropped', () => {
  assert.equal(run('0100|1011'), 1)
})

test('the decode table the IRAM ISR reads is in DRAM', () => {
  assert.match(readFileSync(SLOT, 'utf8'), /static const DRAM_ATTR int8_t s_\{\{prefix_lc\}\}_qdec\[16\]/)
})
