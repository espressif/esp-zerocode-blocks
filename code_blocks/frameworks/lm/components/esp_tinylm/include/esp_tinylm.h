/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_tinylm.h
 * @brief Tiny language-model inference for ESP32 (Per-Layer-Embeddings + PIE).
 *
 * `esp_tinylm` runs small transformer language models on ESP32-class chips. It
 * pairs a memory-efficient architecture (Google Gemma's Per-Layer-Embeddings:
 * a tiny compute core plus a large, flash-resident lookup table) with a
 * hand-written vector kernel to get coherent output at interactive speed.
 *
 * The inference core is a clean-room reimplementation on top of the structure of
 * Andrej Karpathy's llama2.c (https://github.com/karpathy/llama2.c).
 *
 * Portability: the accelerated int4 matvec uses ESP32-S31 PIE (`xespv`) assembly.
 * On other targets (ESP32-S3, ESP32-P4, host) the same API runs a portable
 * scalar path, so code written against this header builds and runs everywhere;
 * only the S31 gets the vector speedup.
 *
 * Typical use:
 * @code
 *   extern const uint8_t model_bin_start[] asm("_binary_model_bin_start");
 *   esp_tinylm_model_handle_t lm;
 *   ESP_ERROR_CHECK(esp_tinylm_load(model_bin_start, &lm));
 *   esp_tinylm_enable_accel();                 // from the inference task
 *   const float *logits;
 *   ESP_ERROR_CHECK(esp_tinylm_forward(lm, token, pos, &logits));  // per step
 * @endcode
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to a loaded model (owns the weight binding and scratch). */
typedef struct esp_tinylm_model_s *esp_tinylm_model_handle_t;

/** @brief Static description of a loaded model. */
typedef struct {
    int vocab_size;   /*!< Output vocabulary size. */
    int dim;          /*!< Model (embedding) dimension. */
    int n_layers;     /*!< Number of transformer layers. */
    int seq_len;      /*!< Maximum context length, in positions. */
} esp_tinylm_info_t;

/**
 * @brief Bind a model from an in-place model blob and allocate its scratch.
 *
 * The blob (produced by the export tooling) is bound in place — no copy is made,
 * so it is typically embedded in the application image or memory-mapped from
 * flash, keeping the large per-layer table flash-resident. Scratch buffers and
 * the KV cache are allocated in PSRAM.
 *
 * @param[in]  blob  Pointer to the start of a model blob.
 * @param[out] out   Receives the model handle on success.
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p blob or @p out is NULL
 *      - ESP_ERR_INVALID_VERSION if the blob is not a recognized model
 *      - ESP_ERR_NO_MEM if scratch allocation fails
 */
esp_err_t esp_tinylm_load(const void *blob, esp_tinylm_model_handle_t *out);

/**
 * @brief Free a model handle and release its scratch.
 * @param[in] model  Handle from esp_tinylm_load(), or NULL (ignored).
 */
void esp_tinylm_free(esp_tinylm_model_handle_t model);

/**
 * @brief Query the static description of a loaded model.
 * @param[in]  model  Model handle.
 * @param[out] info   Receives the model description.
 * @return ESP_OK, or ESP_ERR_INVALID_ARG.
 */
esp_err_t esp_tinylm_get_info(esp_tinylm_model_handle_t model, esp_tinylm_info_t *info);

/**
 * @brief Enable the hardware vector unit for inference on the current task.
 *
 * On ESP32-S31 this enables the PIE unit (core-1 only) so matmuls use the
 * accelerated int4 kernel; call it once from the task that runs inference and
 * pin that task to core 1. On other targets it is a no-op (the scalar path is
 * used). Safe to call unconditionally.
 */
void esp_tinylm_enable_accel(void);

/**
 * @brief Run one autoregressive decode step.
 *
 * Feeds @p token at sequence position @p pos, updates the KV cache, and returns
 * the output logits for the next token. The returned buffer is owned by @p model
 * and stays valid until the next call on the same handle.
 *
 * @param[in]  model   Model handle.
 * @param[in]  token   Input token id, in [0, vocab_size).
 * @param[in]  pos     Sequence position, in [0, seq_len); increment by one per step.
 * @param[out] logits  Receives a pointer to @c vocab_size floats.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, or ESP_ERR_INVALID_STATE.
 */
esp_err_t esp_tinylm_forward(esp_tinylm_model_handle_t model, int token, int pos,
                             const float **logits);

#ifdef __cplusplus
}
#endif
