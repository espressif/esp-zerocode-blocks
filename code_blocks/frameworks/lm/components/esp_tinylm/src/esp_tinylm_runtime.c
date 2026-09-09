/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "esp_tinylm_runtime.h"

/* Advance p so its offset from the 16-aligned blob base is a multiple of 16;
 * matches the padding the exporter/repack tool insert before every region so
 * quantized rows are 16-byte aligned for the SIMD load (ee.vld.128 on the S3). */
static const uint8_t *align16(const uint8_t *base, const uint8_t *p)
{
    size_t off = (size_t)(p - base);
    return p + ((16 - (off & 15)) & 15);
}

static const uint8_t *bind_q(const uint8_t *base, const uint8_t *p, tinylm_qt_t *t,
                             int rows, int cols, int group)
{
    t->rows = rows;
    t->cols = cols;
    t->n_groups = (cols + group - 1) / group;
    t->row_bytes = (cols + 1) / 2;
    p = align16(base, p);
    t->codes = p;
    p += (size_t)rows * t->row_bytes;
    p = align16(base, p);
    t->scales = (const uint16_t *)p;
    p += (size_t)rows * t->n_groups * 2;
    return p;
}

static const uint8_t *bind_f(const uint8_t *base, const uint8_t *p, const float **t, int n)
{
    p = align16(base, p);
    *t = (const float *)p;
    return p + (size_t)n * 4;
}

/** Dequantize row r of a tensor to fp32 (used for the on-the-fly embedding lookups). */
static void deq_row(const tinylm_qt_t *t, int r, float *out, int group)
{
    const uint8_t *row = t->codes + (size_t)r * t->row_bytes;
    const uint16_t *sc = t->scales + (size_t)r * t->n_groups;
    for (int gi = 0; gi < t->n_groups; gi++) {
        int a = gi * group, b = a + group;
        if (b > t->cols) {
            b = t->cols;
        }
        float scale = tinylm_h2f(sc[gi]);
        for (int j = a; j < b; j++) {
            uint8_t byte = row[j >> 1];
            int code = (j & 1) ? (byte >> 4) : (byte & 0xF);
            out[j] = (float)(code - 8) * scale;
        }
    }
}

void tinylm_quant_act(const float *x, int n, int8_t *xq, float *xs)
{
    float mx = 1e-8f;
    for (int j = 0; j < n; j++) {
        float a = fabsf(x[j]);
        if (a > mx) {
            mx = a;
        }
    }
    float inv = 127.f / mx;
    for (int j = 0; j < n; j++) {
        int q = (int)lrintf(x[j] * inv);
        xq[j] = (int8_t)(q > 127 ? 127 : (q < -127 ? -127 : q));
    }
    *xs = mx / 127.f;
}

static void rmsnorm(const float *x, const float *w, int n, float *o)
{
    float ss = 0.f;
    for (int i = 0; i < n; i++) {
        ss += x[i] * x[i];
    }
    float inv = 1.f / sqrtf(ss / n + TINYLM_EPS);
    for (int i = 0; i < n; i++) {
        o[i] = w[i] * x[i] * inv;
    }
}

static inline float silu(float x)
{
    return x / (1.f + expf(-x));
}

int tinylm_bind(const uint8_t *base, tinylm_weights_t *w)
{
    const uint8_t *p = base;
    uint32_t magic;
    memcpy(&magic, p, 4);
    p += 4;
    if (magic != TINYLM_MAGIC) {
        return -1;
    }
    int32_t hv[8];
    memcpy(hv, p, 32);
    p += 32;
    w->c.vocab = hv[0];
    w->c.dim = hv[1];
    w->c.n_layers = hv[2];
    w->c.n_heads = hv[3];
    w->c.ffn = hv[4];
    w->c.ple_dim = hv[5];
    w->c.seq_len = hv[6];
    w->c.group = hv[7];
    memcpy(&w->c.rope_theta, p, 4);
    p += 4;
    int D = w->c.dim, L = w->c.n_layers, P = w->c.ple_dim, F = w->c.ffn, V = w->c.vocab, G = w->c.group;
    p = bind_q(base, p, &w->tok_emb, V, D, G);
    p = bind_q(base, p, &w->ple_table, V, L * P, G);
    p = bind_f(base, p, &w->ple_norm, P);
    for (int i = 0; i < L; i++) {
        p = bind_q(base, p, &w->ple_inject[i], D, P, G);
        p = bind_f(base, p, &w->attn_norm[i], D);
        p = bind_q(base, p, &w->qkv[i], 3 * D, D, G);
        p = bind_q(base, p, &w->attn_proj[i], D, D, G);
        p = bind_f(base, p, &w->ffn_norm[i], D);
        p = bind_q(base, p, &w->gate[i], F, D, G);
        p = bind_q(base, p, &w->up[i], F, D, G);
        p = bind_q(base, p, &w->down[i], D, F, G);
    }
    p = bind_f(base, p, &w->out_norm, D);
    return 0;
}

void tinylm_forward(tinylm_weights_t *w, tinylm_scratch_t *s, int token, int pos)
{
    int D = w->c.dim, L = w->c.n_layers, P = w->c.ple_dim, F = w->c.ffn;
    int H = w->c.n_heads, Dh = D / H, S = w->c.seq_len, G = w->c.group;

    deq_row(&w->tok_emb, token, s->x, G);
    deq_row(&w->ple_table, token, s->trow, G);            /* [L*P] */

    float *rc = s->rope, *rs = s->rope + Dh / 2;          /* RoPE cos/sin for this pos */
    for (int i = 0; i < Dh / 2; i++) {
        float fr = powf(w->c.rope_theta, -2.f * i / Dh);
        rc[i] = cosf(pos * fr);
        rs[i] = sinf(pos * fr);
    }

    for (int l = 0; l < L; l++) {
        /* PLE injection */
        rmsnorm(s->trow + l * P, w->ple_norm, P, s->tmpP);
        tinylm_matvec(&w->ple_inject[l], s->tmpP, s->h, G);   /* [D] */
        for (int i = 0; i < D; i++) {
            s->x[i] += s->h[i];
        }

        /* attention */
        rmsnorm(s->x, w->attn_norm[l], D, s->h);
        tinylm_matvec(&w->qkv[l], s->h, s->qkv, G);           /* [3D] */
        float *q = s->qkv, *k = s->qkv + D, *v = s->qkv + 2 * D;
        for (int hh = 0; hh < H; hh++) {
            float *qh = q + hh * Dh, *kh = k + hh * Dh;
            for (int i = 0; i < Dh / 2; i++) {
                float c = rc[i], sn = rs[i];
                float q1 = qh[i], q2 = qh[i + Dh / 2];
                qh[i] = q1 * c - q2 * sn;
                qh[i + Dh / 2] = q2 * c + q1 * sn;
                float k1 = kh[i], k2 = kh[i + Dh / 2];
                kh[i] = k1 * c - k2 * sn;
                kh[i + Dh / 2] = k2 * c + k1 * sn;
            }
        }
        float *kc = s->kcache + (size_t)l * S * D, *vc = s->vcache + (size_t)l * S * D;
        memcpy(kc + (size_t)pos * D, k, D * 4);
        memcpy(vc + (size_t)pos * D, v, D * 4);
        float scale = 1.f / sqrtf((float)Dh);
        for (int hh = 0; hh < H; hh++) {
            float *qh = q + hh * Dh, *ao = s->att + hh * Dh;
            for (int i = 0; i < Dh; i++) {
                ao[i] = 0.f;
            }
            float mx = -1e30f;
            for (int t = 0; t <= pos; t++) {
                float *kt = kc + (size_t)t * D + hh * Dh, d = 0.f;
                for (int i = 0; i < Dh; i++) {
                    d += qh[i] * kt[i];
                }
                d *= scale;
                s->scores[t] = d;
                if (d > mx) {
                    mx = d;
                }
            }
            float den = 0.f;
            for (int t = 0; t <= pos; t++) {
                float wgt = expf(s->scores[t] - mx);
                den += wgt;
                float *vt = vc + (size_t)t * D + hh * Dh;
                for (int i = 0; i < Dh; i++) {
                    ao[i] += wgt * vt[i];
                }
            }
            for (int i = 0; i < Dh; i++) {
                ao[i] /= den;
            }
        }
        tinylm_matvec(&w->attn_proj[l], s->att, s->h, G);
        for (int i = 0; i < D; i++) {
            s->x[i] += s->h[i];
        }

        /* SwiGLU FFN */
        rmsnorm(s->x, w->ffn_norm[l], D, s->h);
        tinylm_matvec(&w->gate[l], s->h, s->g1, G);
        tinylm_matvec(&w->up[l], s->h, s->g2, G);
        for (int i = 0; i < F; i++) {
            s->g1[i] = silu(s->g1[i]) * s->g2[i];
        }
        tinylm_matvec(&w->down[l], s->g1, s->h, G);
        for (int i = 0; i < D; i++) {
            s->x[i] += s->h[i];
        }
    }

    rmsnorm(s->x, w->out_norm, D, s->x);
    tinylm_matvec(&w->tok_emb, s->x, s->logits, G);       /* tied head */
}
