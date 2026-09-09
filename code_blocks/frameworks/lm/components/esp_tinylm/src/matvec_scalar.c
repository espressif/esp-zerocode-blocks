/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Portable scalar int4xint8 matvec. Built for every target that does not have a
 * dedicated accelerated kernel (S3/P4/host today). Quantize the activations to
 * int8 once, then per row accumulate int8*int4 -> int32 group dots scaled by
 * (x_scale * group_scale).
 */

#include "esp_tinylm_runtime.h"

void tinylm_matvec(const tinylm_qt_t *w, const float *x, float *y, int group)
{
    int8_t xq[512];
    float xs;
    tinylm_quant_act(x, w->cols, xq, &xs);
    for (int r = 0; r < w->rows; r++) {
        const uint8_t *row = w->codes + (size_t)r * w->row_bytes;
        const uint16_t *sc = w->scales + (size_t)r * w->n_groups;
        float acc = 0.f;
        for (int gi = 0; gi < w->n_groups; gi++) {
            int a = gi * group, b = a + group;
            if (b > w->cols) {
                b = w->cols;
            }
            int32_t g = 0;
            for (int j = a; j < b; j++) {
                uint8_t byte = row[j >> 1];
                int code = (j & 1) ? (byte >> 4) : (byte & 0xF);
                g += (code - 8) * (int)xq[j];
            }
            acc += (float)g * tinylm_h2f(sc[gi]);
        }
        y[r] = acc * xs;
    }
}

void tinylm_enable_accel(void)
{
    /* no hardware vector unit on this target; scalar path is always active */
}
