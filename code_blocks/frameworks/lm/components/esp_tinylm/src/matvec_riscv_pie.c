/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Accelerated int4xint8 matvec using the PIE (xespv) RISC-V vector unit, shared
 * by the ESP32-S31 and ESP32-P4 (same PIE ISA).
 *
 * Each 16-byte chunk of packed int4 weights is burst-loaded (esp.vld.128) and
 * unpacked to two int8 vectors in-register: low = q & 0x0F, high =
 * (esp.vsr.s32 q,4) & 0x0F (the 32-bit shift crosses byte boundaries but the
 * mask strips the contamination). The weights stay in the standard interleaved
 * layout, so activations are deinterleaved once per matvec (even/odd) to pair
 * with the low/high nibbles, and MAC'd with esp.vmulas.s8.xacc. Codes are
 * value+8; each group dot subtracts 8*sum(act). The esp.* GPR operands are held
 * in x28-x31 (t3-t6), which the xespv rule requires (operands must be x26-x31).
 *
 * Compiled for CONFIG_IDF_TARGET_ESP32S31 and _ESP32P4 (see CMakeLists). PIE is
 * core-1-only on the S31; the P4 path is build-verified but not yet HW-checked.
 */

#include "esp_tinylm_runtime.h"

static const int8_t k_mask0f[16] __attribute__((aligned(16))) = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15
};

void tinylm_enable_accel(void)
{
    asm volatile(
        "csrsi  0x7f2, 0b01     \n\t"
        "li     x29, 0b10       \n\t"
        "esp.movx.w.cfg x29     \n\t"
        ::: "x29");
}

/* sum_k code[k]*act[k] over n (a multiple of 32) weights, interleaved layout;
 * ev/od are the deinterleaved even/odd activations for this span. */
static inline int32_t pie_dot(const uint8_t *w, const int8_t *ev, const int8_t *od, int n)
{
    int32_t r;
    int chunks = n >> 5;
    asm volatile(
        "esp.zero.xacc               \n\t"
        "li     t3, 4                \n\t"
        "esp.movx.w.sar t3           \n\t"
        "mv     t3, %[m]             \n\t"
        "esp.vld.128.ip q7, t3, 16   \n\t"
        "mv     t4, %[w]             \n\t"
        "mv     t5, %[e]             \n\t"
        "mv     t6, %[o]             \n\t"
        "mv     t3, %[c]             \n\t"
        "1:                          \n\t"
        "esp.vld.128.ip q0, t4, 16   \n\t"   /* 16 packed = 32 codes */
        "esp.andq    q1, q0, q7      \n\t"   /* low  = even codes */
        "esp.vsr.s32 q0, q0          \n\t"
        "esp.andq    q0, q0, q7      \n\t"   /* high = odd codes */
        "esp.vld.128.ip q2, t5, 16   \n\t"   /* even acts */
        "esp.vmulas.s8.xacc q1, q2   \n\t"
        "esp.vld.128.ip q2, t6, 16   \n\t"   /* odd acts */
        "esp.vmulas.s8.xacc q0, q2   \n\t"
        "addi   t3, t3, -1           \n\t"
        "bnez   t3, 1b               \n\t"
        "esp.movx.r.xacc.l t3        \n\t"
        "mv     %[r], t3             \n\t"
        : [r] "=r"(r)
        : [w] "r"(w), [e] "r"(ev), [o] "r"(od), [m] "r"(k_mask0f), [c] "r"(chunks)
        : "t3", "t4", "t5", "t6", "memory");
    return r;
}

void tinylm_matvec(const tinylm_qt_t *w, const float *x, float *y, int group)
{
    static int8_t xq[512] __attribute__((aligned(16)));
    static int8_t ev[256] __attribute__((aligned(16)));
    static int8_t od[256] __attribute__((aligned(16)));
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
        float acc = 0.f;
        for (int gi = 0; gi < w->n_groups; gi++) {
            int a = gi * group, b = a + group;
            if (b > cols) {
                b = cols;
            }
            int32_t d = pie_dot(row + (a >> 1), ev + (a >> 1), od + (a >> 1), b - a);
            acc += (float)(d - 8 * gsum[gi]) * tinylm_h2f(sc[gi]);
        }
        y[r] = acc * xs;
    }
}
