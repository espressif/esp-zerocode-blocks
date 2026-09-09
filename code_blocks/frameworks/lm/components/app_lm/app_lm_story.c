/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI — free-text generation (story model)
 *
 * Compiled only when CONFIG_ZC_LM_MODEL_STORY is set. Adapted from
 * esp-tinylm's story_engine.h.
 *
 * DIFFERENT FROM THE COMMAND PARSER in three ways that matter:
 *   - a BPE subword tokenizer, not the command model's byte tokenizer
 *   - temperature SAMPLING, not a constrained argmax — output is open-ended,
 *     so nothing bounds it to a schema and nothing should be wired straight to
 *     hardware from it
 *   - it streams, because at ~26.7 tok/s a paragraph takes seconds
 */

#include "app_lm.h"
#include "esp_tinylm.h"
#include "bpe.h"

#include <esp_log.h>
#include <math.h>
#include <stdint.h>   /* UINT32_MAX for the sampler */
#include <esp_random.h>

static const char *TAG = "app_lm_story";

extern const uint8_t story_bin_start[] asm("_binary_story_bin_start");

static esp_tinylm_model_handle_t s_story;

esp_err_t zc_lm_story_init(void)
{
    if (s_story != NULL) {
        return ESP_OK;
    }
    esp_err_t err = esp_tinylm_load(story_bin_start, &s_story);
    if (err != ESP_OK) {
        /* 2 MB of weights plus scratch and a KV cache — this is the allocation
         * most likely to fail on a part with modest PSRAM. */
        ESP_LOGE(TAG, "story model load failed: %s", esp_err_to_name(err));
        s_story = NULL;
        return err;
    }
    esp_tinylm_info_t info;
    if (esp_tinylm_get_info(s_story, &info) == ESP_OK) {
        ESP_LOGI(TAG, "story model ready — vocab %d, dim %d, %d layers, context %d",
                 info.vocab_size, info.dim, info.n_layers, info.seq_len);
    }
    return ESP_OK;
}

void zc_lm_story_run(const char *prompt, int max_new, float temp,
                     app_lm_text_cb_t cb, void *arg)
{
    if (s_story == NULL || prompt == NULL || cb == NULL) {
        return;
    }
    if (temp <= 0.0f) {
        temp = 0.8f;      /* 0 would divide by zero below; 0.8 suits this size */
    }

    esp_tinylm_info_t info;
    if (esp_tinylm_get_info(s_story, &info) != ESP_OK) {
        return;
    }
    const int vocab = info.vocab_size;

    /* static: BPE_NVOCAB floats is far too much for a task stack, and only this
     * one worker task ever runs generation. */
    static float pr[BPE_NVOCAB];
    int ids[256];
    int np = bpe_encode(prompt, ids, (int)(sizeof(ids) / sizeof(ids[0])));

    int pos = 0;
    const float *logits = NULL;
    for (int i = 0; i < np && pos < info.seq_len; i++) {
        if (esp_tinylm_forward(s_story, ids[i], pos++, &logits) != ESP_OK) {
            return;
        }
    }

    char tb[32];
    int produced = 0;
    while (produced < max_new && pos < info.seq_len && logits != NULL) {
        /* softmax over the vocabulary at `temp`, shifted by the max for
         * numerical stability, then sample from the distribution. */
        float mx = -1e30f;
        for (int i = 0; i < vocab; i++) {
            if (logits[i] > mx) {
                mx = logits[i];
            }
        }
        float sum = 0.0f;
        for (int i = 0; i < vocab; i++) {
            pr[i] = expf((logits[i] - mx) / temp);
            sum += pr[i];
        }

        /* esp_random() rather than rand(): a story that is identical on every
         * boot is a worse demo than one that is not, and the hardware RNG is
         * already there. */
        float r = ((float)esp_random() / (float)UINT32_MAX) * sum;
        float c = 0.0f;
        int next = vocab - 1;
        for (int i = 0; i < vocab; i++) {
            c += pr[i];
            if (r <= c) {
                next = i;
                break;
            }
        }
        if (next == BPE_EOS) {
            break;
        }

        int n = bpe_decode_append(next, tb, 0, (int)sizeof(tb));
        if (n > 0) {
            cb(tb, n, arg);
        }
        if (esp_tinylm_forward(s_story, next, pos++, &logits) != ESP_OK) {
            break;
        }
        produced++;
    }
    ESP_LOGI(TAG, "generated %d tokens", produced);
}
