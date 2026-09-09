/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI — on-device language model framework
 *
 * Turns a natural-language command into a structured action, offline, with no
 * cloud round trip. Wraps esp_tinylm (Per-Layer-Embeddings llama2 + per-target
 * vector assembly); driver blocks receive the parsed action and drive the param
 * bus, exactly as app_tflite hands driver blocks an inference result.
 *
 * The model is a smart-home command parser: it emits ONE of 540 schema-valid
 * tool-calls of the form
 *
 *     {"device":"light","location":"kitchen","action":"on"}
 *     {"device":"ac","location":"bedroom","action":"set","value":22}
 *
 * Output is CONSTRAINED to that set during decoding, so it is always
 * well-formed and in-schema — there is no JSON parse that can fail, and no
 * hallucinated device. The schema and the model are trained together; changing
 * one means retraining.
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Longest tool-call the schema can emit, plus a terminator. */
#define APP_LM_MAX_JSON 96
/** Longest command accepted. Prompt is "<cmd> => " and the model's context is
 *  finite, so a longer command is truncated rather than silently misparsed. */
#define APP_LM_MAX_CMD  160

/**
 * A parsed action, already matched to one listener's device+location.
 *
 * @param action     "on", "off" or "set" — from the schema, never arbitrary.
 * @param value      the set-point when @p has_value, else 0.
 * @param has_value  whether the tool-call carried a "value" field.
 */
typedef void (*app_lm_cb_t)(const char *action, int value, bool has_value, void *arg);

/**
 * Streamed text from a generative model. Called once per decoded token, on the
 * inference worker — @p chunk is NOT null-terminated and is only valid for the
 * duration of the call. Streaming rather than returning a whole string is
 * deliberate: at 26.7 tok/s a paragraph takes seconds, and text that appears as
 * it is written reads far better than a long pause and a wall.
 */
typedef void (*app_lm_text_cb_t)(const char *chunk, int len, void *arg);

/**
 * Framework init. Binds the embedded model and starts the inference worker.
 *
 * The worker is PINNED TO CORE 1 and calls esp_tinylm_enable_accel() there,
 * because on ESP32-S31 the PIE vector unit is enabled per-core and only core 1
 * has it. Getting that wrong does not fail — it silently falls back to the
 * scalar path and runs several times slower, which is why the framework owns
 * the task rather than leaving each driver to remember.
 */
esp_err_t app_lm_init(void);

/**
 * Route tool-calls for one device+location to a callback.
 *
 * Match is exact against the schema's own spellings ("light", "kitchen").
 * Registering a pair the schema cannot produce is not an error here — it simply
 * never fires — so the block that owns the pair validates it at catalog time.
 *
 * @return ESP_ERR_NO_MEM when the listener table is full (raise
 *         CONFIG_APP_LM_MAX_LISTENERS), ESP_ERR_INVALID_ARG on a NULL argument.
 */
esp_err_t app_lm_register(const char *device, const char *location,
                          app_lm_cb_t cb, void *arg);

/**
 * Queue a command for parsing on the inference worker. Returns as soon as it is
 * queued — inference takes tens to hundreds of milliseconds, so this must not
 * run on a console or event task.
 *
 * @return ESP_ERR_INVALID_STATE before app_lm_init(), ESP_ERR_NO_MEM if the
 *         queue is full (one command is in flight at a time by design).
 */
esp_err_t app_lm_submit(const char *cmd);

/**
 * Parse synchronously, for a console command or a test. Blocks for the whole
 * decode. Writes the raw tool-call JSON to @p out and returns its length, or a
 * negative value on failure. Does NOT dispatch to listeners.
 */
int app_lm_parse(const char *cmd, char *out, int outsz);

/**
 * Queue free-text generation on the inference worker (story model).
 *
 * @param max_new  token budget — generation also stops at end-of-sequence or
 *                 the model's context limit, whichever comes first.
 * @param temp     sampling temperature. Lower is more predictable; ~0.8 is a
 *                 reasonable default for TinyStories-scale models.
 * @return ESP_ERR_NOT_SUPPORTED when the story model was not built in
 *         (CONFIG_ZC_LM_MODEL_STORY), ESP_ERR_NO_MEM when busy.
 */
esp_err_t app_lm_submit_story(const char *prompt, int max_new, float temp,
                              app_lm_text_cb_t cb, void *arg);

#ifdef __cplusplus
}
#endif
