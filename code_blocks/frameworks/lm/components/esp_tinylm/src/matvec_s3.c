/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ESP32-S3 accelerated int4xint8 matvec using the Espressif SIMD/Vector
 * Extension (the LX7 128-bit EE.* instructions).
 *
 * Each 16-byte chunk of packed int4 weights is loaded (ee.vld.128) and unpacked
 * in-register to two int8 vectors -- low = q & 0x0F, high = (ee.vsr.32 q by 4) &
 * 0x0F (the 32-bit lane shift crosses byte boundaries but the mask strips the
 * contamination). The low/high nibbles pair with the deinterleaved even/odd
 * activations and MAC into the 40-bit ACCX with ee.vmulas.s8.accx. Codes are
 * value+8; each group dot subtracts 8*sum(act). Compiled only for the S3.
 *
 * Unlike the S31 PIE load, ee.vld.128.ip requires a 16-byte aligned address. The
 * PLE3 blob is laid out so every weight row is 16-byte aligned in flash (16-aligned
 * base + 16-padded regions), so rows load straight from flash with no copy. As a
 * safety net for a hypothetically misaligned blob, an unaligned row is bounced
 * through a 16-aligned scratch. group is always a multiple of 32, so every group
 * offset into an aligned row stays aligned.
 *
 * The S3 vector unit is not core-restricted (unlike the S31 PIE) and is enabled
 * by IDF, so tinylm_enable_accel() is a no-op here.
 */

#include <string.h>
#include "esp_tinylm_runtime.h"

static const int8_t k_mask0f[16] __attribute__((aligned(16))) = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15
};

void tinylm_enable_accel(void)
{
    /* S3 vector coprocessor is enabled by the system; nothing to do */
}

/* sum_k code[k]*act[k] over n (a multiple of 32) weights, interleaved layout;
 * w must be 16-byte aligned; ev/od are the deinterleaved even/odd activations. */
static inline int32_t s3_dot(const uint8_t *w, const int8_t *ev, const int8_t *od, int n)
{
    int32_t r;
    int chunks = n >> 5;
    asm volatile(
        "ee.zero.accx                    \n"
        "movi.n  a13, 4                  \n"   /* shift amount held live in a13 */
        "mov     a8, %[m]                \n"
        "ee.vld.128.ip q7, a8, 16        \n"   /* mask = 0x0F x16 */
        "mov     a8, %[w]                \n"
        "mov     a9, %[e]                \n"
        "mov     a10, %[o]               \n"
        "mov     a11, %[c]               \n"
        "1:                              \n"
        "ee.vld.128.ip q0, a8, 16        \n"   /* 16 packed = 32 codes */
        "ee.andq   q1, q0, q7            \n"   /* low  = even codes */
        "wsr.sar   a13                   \n"   /* SAR = 4 for ee.vsr.32 */
        "ee.vsr.32 q0, q0               \n"
        "ee.andq   q0, q0, q7            \n"   /* high = odd codes */
        "ee.vld.128.ip q2, a9, 16        \n"   /* even acts */
        "ee.vmulas.s8.accx q1, q2        \n"
        "ee.vld.128.ip q2, a10, 16       \n"   /* odd acts */
        "ee.vmulas.s8.accx q0, q2        \n"
        "addi.n  a11, a11, -1            \n"
        "bnez    a11, 1b                 \n"
        "rur.accx_0 %[r]                 \n"
        : [r] "=r"(r)
        : [w] "r"(w), [e] "r"(ev), [o] "r"(od), [m] "r"(k_mask0f), [c] "r"(chunks)
        : "a8", "a9", "a10", "a11", "a13", "memory");
    return r;
}

void tinylm_matvec(const tinylm_qt_t *w, const float *x, float *y, int group)
{
    static int8_t xq[512] __attribute__((aligned(16)));
    static int8_t ev[256] __attribute__((aligned(16)));
    static int8_t od[256] __attribute__((aligned(16)));
    static uint8_t arow[512] __attribute__((aligned(16)));
    float xs;
    int cols = w->cols;
    tinylm_quant_act(x, cols, xq, &xs);
    int half = cols >> 1;
    for (int j = 0; j < half; j++) {
        ev[j] = xq[2 * j];
        od[j] = xq[2 * j + 1];
    }
    /* group activation sums depend only on x -> compute once, reuse over rows */
    int gsum[TINYLM_MAXL * 8];
    for (int gi = 0; gi < w->n_groups; gi++) {
        int a = gi * group, b = a + group;
        if (b > cols) {
            b = cols;
        }
        int sum = 0;
        for (int j = a; j < b; j++) {
            sum += xq[j];
        }
        gsum[gi] = sum;
    }
    for (int r = 0; r < w->rows; r++) {
        const uint8_t *row = w->codes + (size_t)r * w->row_bytes;
        const uint16_t *sc = w->scales + (size_t)r * w->n_groups;
        /* PLE3 rows are 16-byte aligned in flash -> load in place; only bounce
         * through the aligned scratch if a blob ever hands us a misaligned row. */
        const uint8_t *rp = row;
        if (((uintptr_t)row & 15) != 0) {
            memcpy(arow, row, w->row_bytes);
            rp = arow;
        }
        float acc = 0.f;
        for (int gi = 0; gi < w->n_groups; gi++) {
            int a = gi * group, b = a + group;
            if (b > cols) {
                b = cols;
            }
            int32_t d = s3_dot(rp + (a >> 1), ev + (a >> 1), od + (a >> 1), b - a);
            acc += (float)(d - 8 * gsum[gi]) * tinylm_h2f(sc[gi]);
        }
        y[r] = acc * xs;
    }
}
