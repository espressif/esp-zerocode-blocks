/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Internal runtime for esp_tinylm — NOT a public header.
 *
 * Clean-room inference for the PLE TinyLM: RMSNorm, split-half RoPE, SwiGLU FFN,
 * tied input/output embedding, and the Per-Layer-Embeddings injection
 *   x += ple_inject_l( RMSNorm( ple_table(tok)[l] ) )  at each layer.
 * Structure follows Andrej Karpathy's llama2.c; the PLE table is the Gemma trick.
 *
 * The int4 matmul is factored out behind tinylm_matvec()/tinylm_enable_accel(),
 * which each target implements in its own src/matvec_*.c (scalar / S31 PIE / ...).
 */
#pragma once

#include <stdint.h>
#include <string.h>

#define TINYLM_MAGIC 0x504C4533u   /* 'PLE3' -- 16-byte aligned regions */
#define TINYLM_EPS   1e-6f
#define TINYLM_MAXL  16

/** Model hyperparameters, parsed from the blob header. */
typedef struct {
    int vocab, dim, n_layers, n_heads, ffn, ple_dim, seq_len, group;
    float rope_theta;
} tinylm_cfg_t;

/** A group-wise int4-quantized weight tensor bound in place. */
typedef struct {
    const uint8_t *codes;    /* rows*row_bytes, nibble = value+8 */
    const uint16_t *scales;  /* rows*n_groups, fp16 */
    int rows, cols, n_groups, row_bytes;
} tinylm_qt_t;

/** Bound model weights (pointers into the blob). */
typedef struct {
    tinylm_cfg_t c;
    tinylm_qt_t tok_emb;               /* [V,D] tied head */
    tinylm_qt_t ple_table;             /* [V,L*P] */
    const float *ple_norm;             /* [P] */
    tinylm_qt_t ple_inject[TINYLM_MAXL];
    const float *attn_norm[TINYLM_MAXL];
    tinylm_qt_t qkv[TINYLM_MAXL], attn_proj[TINYLM_MAXL];
    const float *ffn_norm[TINYLM_MAXL];
    tinylm_qt_t gate[TINYLM_MAXL], up[TINYLM_MAXL], down[TINYLM_MAXL];
    const float *out_norm;             /* [D] */
} tinylm_weights_t;

/** Per-forward scratch + persistent KV cache. */
typedef struct {
    float *x, *h, *qkv, *att, *g1, *g2, *tmpP, *trow, *logits, *scores, *rope;
    float *kcache, *vcache;            /* [L*seq_len*D] each */
} tinylm_scratch_t;

/** fp16 -> fp32. */
static inline float tinylm_h2f(uint16_t h)
{
    uint32_t s = (uint32_t)(h & 0x8000) << 16, e = (h >> 10) & 0x1F, m = h & 0x3FF, f;
    if (e == 0) {
        if (!m) {
            f = s;
        } else {
            e = 127 - 15 + 1;
            while (!(m & 0x400)) {
                m <<= 1;
                e--;
            }
            m &= 0x3FF;
            f = s | (e << 23) | (m << 13);
        }
    } else if (e == 0x1F) {
        f = s | 0x7F800000u | (m << 13);
    } else {
        f = s | ((e - 15 + 127) << 23) | (m << 13);
    }
    float o;
    memcpy(&o, &f, 4);
    return o;
}

/* ---- implemented in esp_tinylm_runtime.c ---- */
int tinylm_bind(const uint8_t *base, tinylm_weights_t *w);
void tinylm_forward(tinylm_weights_t *w, tinylm_scratch_t *s, int token, int pos);
void tinylm_quant_act(const float *x, int n, int8_t *xq, float *xs);

/* ---- implemented per target in src/matvec_*.c ---- */

/** y[rows] = W * x[cols], int8-activation group dot. The accelerated kernel. */
void tinylm_matvec(const tinylm_qt_t *w, const float *x, float *y, int group);

/** Enable the hardware vector unit on this task (S31 PIE); no-op otherwise. */
void tinylm_enable_accel(void);
