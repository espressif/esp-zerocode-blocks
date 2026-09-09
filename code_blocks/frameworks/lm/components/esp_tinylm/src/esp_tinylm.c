/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_tinylm.h"
#include "esp_tinylm_runtime.h"

static const char *TAG = "esp_tinylm";

struct esp_tinylm_model_s {
    tinylm_weights_t w;
    tinylm_scratch_t s;
    void *bufs[13];   /* scratch allocations, for esp_tinylm_free() */
    int n_bufs;
};

static float *lm_alloc(struct esp_tinylm_model_s *m, int n)
{
    size_t bytes = (size_t)n * sizeof(float);
    /* Prefer PSRAM (the KV cache is large); fall back to internal RAM on parts
     * without PSRAM (small models still fit). */
    float *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    }
    if (p) {
        m->bufs[m->n_bufs++] = p;
    }
    return p;
}

esp_err_t esp_tinylm_load(const void *blob, esp_tinylm_model_handle_t *out)
{
    ESP_RETURN_ON_FALSE(blob && out, ESP_ERR_INVALID_ARG, TAG, "null argument");
    *out = NULL;

    struct esp_tinylm_model_s *m = calloc(1, sizeof(*m));
    ESP_RETURN_ON_FALSE(m, ESP_ERR_NO_MEM, TAG, "no memory for handle");

    if (tinylm_bind((const uint8_t *)blob, &m->w) != 0) {
        free(m);
        ESP_LOGE(TAG, "bad model magic (not a PLE2 blob)");
        return ESP_ERR_INVALID_VERSION;
    }

    const tinylm_cfg_t *c = &m->w.c;
    int D = c->dim, L = c->n_layers, P = c->ple_dim, F = c->ffn, V = c->vocab, S = c->seq_len;
    tinylm_scratch_t *s = &m->s;
    s->x = lm_alloc(m, D);
    s->h = lm_alloc(m, F > D ? F : D);
    s->qkv = lm_alloc(m, 3 * D);
    s->att = lm_alloc(m, D);
    s->g1 = lm_alloc(m, F);
    s->g2 = lm_alloc(m, F);
    s->tmpP = lm_alloc(m, P);
    s->trow = lm_alloc(m, L * P);
    s->logits = lm_alloc(m, V);
    s->scores = lm_alloc(m, S);
    s->rope = lm_alloc(m, D);
    s->kcache = lm_alloc(m, (size_t)L * S * D);
    s->vcache = lm_alloc(m, (size_t)L * S * D);

    for (int i = 0; i < m->n_bufs; i++) {
        if (!m->bufs[i]) {
            esp_tinylm_free(m);
            ESP_LOGE(TAG, "scratch allocation failed");
            return ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGI(TAG, "loaded: V=%d D=%d L=%d seq=%d", V, D, L, S);
    *out = m;
    return ESP_OK;
}

void esp_tinylm_free(esp_tinylm_model_handle_t model)
{
    if (!model) {
        return;
    }
    for (int i = 0; i < model->n_bufs; i++) {
        free(model->bufs[i]);
    }
    free(model);
}

esp_err_t esp_tinylm_get_info(esp_tinylm_model_handle_t model, esp_tinylm_info_t *info)
{
    ESP_RETURN_ON_FALSE(model && info, ESP_ERR_INVALID_ARG, TAG, "null argument");
    info->vocab_size = model->w.c.vocab;
    info->dim = model->w.c.dim;
    info->n_layers = model->w.c.n_layers;
    info->seq_len = model->w.c.seq_len;
    return ESP_OK;
}

void esp_tinylm_enable_accel(void)
{
    tinylm_enable_accel();
}

esp_err_t esp_tinylm_forward(esp_tinylm_model_handle_t model, int token, int pos,
                             const float **logits)
{
    ESP_RETURN_ON_FALSE(model && logits, ESP_ERR_INVALID_ARG, TAG, "null argument");
    ESP_RETURN_ON_FALSE(token >= 0 && token < model->w.c.vocab && pos >= 0 && pos < model->w.c.seq_len,
                        ESP_ERR_INVALID_ARG, TAG, "token/pos out of range");
    tinylm_forward(&model->w, &model->s, token, pos);
    *logits = model->s.logits;
    return ESP_OK;
}
