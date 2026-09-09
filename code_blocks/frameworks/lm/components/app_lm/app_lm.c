/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI — on-device language model framework
 *
 * Self-contained: the generator copies this component verbatim and never
 * regenerates it. Driver blocks reach it through app_lm_register().
 *
 * The decode is CONSTRAINED — adapted from esp-tinylm's nlu_engine.h. At each
 * step the candidate set is narrowed to schema entries still matching the bytes
 * emitted so far, and the model may only pick a byte some candidate allows. The
 * output is therefore always a well-formed, in-schema tool-call: there is no
 * JSON parse that can fail and no device the firmware does not know.
 */

#include "app_lm.h"
#if CONFIG_ZC_LM_MODEL_NLU
#include "nlu_valid.h"
#endif
#include "esp_tinylm.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>   /* atoi, for the schema's numeric "value" field */
#include <stdint.h>

static const char *TAG = "app_lm";

#if CONFIG_ZC_LM_MODEL_STORY
/* Implemented in app_lm_story.c, compiled only when that model is selected. */
esp_err_t zc_lm_story_init(void);
void zc_lm_story_run(const char *prompt, int max_new, float temp,
                     app_lm_text_cb_t cb, void *arg);
#endif

/* Byte-level tokenizer: a token IS a byte. 256 is end-of-sequence. No vocab
 * file, no BPE, no merges table — which is a large part of why this fits. */
#define LM_EOS 256
#define LM_SEP " => "

#ifndef CONFIG_APP_LM_MAX_LISTENERS
#define CONFIG_APP_LM_MAX_LISTENERS 8
#endif

#if CONFIG_ZC_LM_MODEL_NLU
extern const uint8_t nlu_bin_start[] asm("_binary_nlu_bin_start");
#endif

/* One worker serves both models, because the PIE unit is enabled per core and
 * the framework owns the only core-1 task. Jobs say which model they want. */
typedef enum {
    LM_JOB_PARSE,   /* NLU: command -> tool-call, dispatched to listeners */
    LM_JOB_STORY,   /* free generation, streamed to a callback */
} lm_job_kind_t;

typedef struct {
    lm_job_kind_t kind;
    char text[APP_LM_MAX_CMD];
    int max_new;              /* story only */
    float temp;               /* story only */
    app_lm_text_cb_t cb;      /* story only */
    void *arg;
} lm_job_t;

typedef struct {
    const char *device;
    const char *location;
    app_lm_cb_t cb;
    void *arg;
} lm_listener_t;

static esp_tinylm_model_handle_t s_lm;
static lm_listener_t s_listeners[CONFIG_APP_LM_MAX_LISTENERS];
static int s_nlisteners;
static QueueHandle_t s_queue;

/* ── the constrained decode ──────────────────────────────────────────────── */
#if CONFIG_ZC_LM_MODEL_NLU

int app_lm_parse(const char *cmd, char *out, int outsz)
{
    if (s_lm == NULL || cmd == NULL || out == NULL || outsz < 2) {
        return -1;
    }
    esp_tinylm_info_t info;
    if (esp_tinylm_get_info(s_lm, &info) != ESP_OK) {
        return -1;
    }

    /* Candidate set: every schema entry, narrowed as bytes are committed.
     * static because 540 ints is 2 KB and this runs on one task only. */
    static int cand[NLU_NVALID];
    int nc = NLU_NVALID;
    for (int i = 0; i < nc; i++) {
        cand[i] = i;
    }

    /* Prime the model with "<command> => " one byte at a time. */
    char prompt[APP_LM_MAX_CMD + 8];
    int plen = snprintf(prompt, sizeof(prompt), "%s%s", cmd, LM_SEP);
    if (plen < 0) {
        return -1;
    }
    if (plen >= (int)sizeof(prompt)) {
        plen = (int)sizeof(prompt) - 1;   /* truncated, not misparsed */
    }

    int pos = 0;
    const float *logits = NULL;
    for (int i = 0; i < plen && pos < info.seq_len; i++) {
        if (esp_tinylm_forward(s_lm, (unsigned char)prompt[i], pos++, &logits) != ESP_OK) {
            return -1;
        }
    }

    int oi = 0;
    while (oi < outsz - 1 && pos < info.seq_len && logits != NULL) {
        /* Which bytes could still lead to a valid tool-call? */
        char allowed[256];
        memset(allowed, 0, sizeof(allowed));
        int eos_ok = 0;
        for (int k = 0; k < nc; k++) {
            const char *sv = NLU_VALID[cand[k]];
            int len = (int)strlen(sv);
            if (len == oi) {
                eos_ok = 1;                  /* a candidate ends exactly here */
            } else if (len > oi) {
                allowed[(unsigned char)sv[oi]] = 1;
            }
        }

        /* Highest-scoring ALLOWED byte — the model chooses, the schema vetoes. */
        int best = -1;
        float bv = -1e30f;
        for (int b = 0; b < 256; b++) {
            if (allowed[b] && logits[b] > bv) {
                bv = logits[b];
                best = b;
            }
        }
        if (eos_ok && logits[LM_EOS] > bv) {
            best = LM_EOS;
        }
        if (best < 0 || best == LM_EOS) {
            break;
        }

        out[oi++] = (char)best;
        if (esp_tinylm_forward(s_lm, best, pos++, &logits) != ESP_OK) {
            break;
        }
        int w = 0;
        for (int k = 0; k < nc; k++) {
            const char *sv = NLU_VALID[cand[k]];
            if ((int)strlen(sv) >= oi && (unsigned char)sv[oi - 1] == (unsigned char)best) {
                cand[w++] = cand[k];
            }
        }
        nc = w;
    }
    out[oi] = '\0';
    return oi;
}

/* ── field extraction ────────────────────────────────────────────────────
 * The output is one of NLU_VALID, so the shape is known exactly and a full
 * JSON parser would be dead weight. Read the quoted value after a key. */
static bool field_str(const char *json, const char *key, char *out, int outsz)
{
    char pat[24];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(json, pat);
    if (p == NULL) {
        return false;
    }
    p += strlen(pat);
    const char *e = strchr(p, '"');
    if (e == NULL || (e - p) >= outsz) {
        return false;
    }
    memcpy(out, p, (size_t)(e - p));
    out[e - p] = '\0';
    return true;
}

static bool field_int(const char *json, const char *key, int *out)
{
    char pat[24];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (p == NULL) {
        return false;
    }
    p += strlen(pat);
    if (*p == '"') {
        return false;              /* a string, not the numeric value field */
    }
    *out = atoi(p);
    return true;
}

static void dispatch(const char *json)
{
    char device[24], location[24], action[16];
    if (!field_str(json, "device", device, sizeof(device)) ||
        !field_str(json, "location", location, sizeof(location)) ||
        !field_str(json, "action", action, sizeof(action))) {
        ESP_LOGW(TAG, "unparsable tool-call: %s", json);
        return;
    }
    int value = 0;
    bool has_value = field_int(json, "value", &value);

    int fired = 0;
    for (int i = 0; i < s_nlisteners; i++) {
        if (strcmp(s_listeners[i].device, device) == 0 &&
            strcmp(s_listeners[i].location, location) == 0) {
            s_listeners[i].cb(action, value, has_value, s_listeners[i].arg);
            fired++;
        }
    }
    if (fired == 0) {
        /* Understood, but nothing in this product owns it. Worth saying: it is
         * the difference between "the model failed" and "you asked for a room
         * this device does not have". */
        ESP_LOGI(TAG, "no listener for %s/%s (action=%s)", device, location, action);
    }
}

#else  /* !CONFIG_ZC_LM_MODEL_NLU — no command model, no schema, no dispatch */
int app_lm_parse(const char *cmd, char *out, int outsz)
{
    (void)cmd; (void)out; (void)outsz;
    return -1;
}
static void dispatch(const char *json) { (void)json; }
#endif

/* ── the worker ─────────────────────────────────────────────────────────── */

static void lm_task(void *arg)
{
    /* Must run HERE, on this task, on core 1: esp_tinylm enables the PIE vector
     * unit per-core and only core 1 has it on ESP32-S31. Called on the wrong
     * core it does not fail — it silently uses the scalar path at a fraction of
     * the speed. */
    esp_tinylm_enable_accel();

    lm_job_t job;
    char json[APP_LM_MAX_JSON];
    for (;;) {
        if (xQueueReceive(s_queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (job.kind) {
        case LM_JOB_PARSE: {
            int n = app_lm_parse(job.text, json, sizeof(json));
            if (n <= 0) {
                ESP_LOGW(TAG, "parse failed for \"%s\"", job.text);
                break;
            }
            ESP_LOGI(TAG, "\"%s\" -> %s", job.text, json);
            dispatch(json);
            break;
        }
        case LM_JOB_STORY:
#if CONFIG_ZC_LM_MODEL_STORY
            zc_lm_story_run(job.text, job.max_new, job.temp, job.cb, job.arg);
#else
            ESP_LOGW(TAG, "story model not built in (CONFIG_ZC_LM_MODEL_STORY)");
#endif
            break;
        }
    }
}

esp_err_t app_lm_init(void)
{
#if CONFIG_ZC_LM_MODEL_NLU
    if (s_lm != NULL) {
        return ESP_OK;
    }
    esp_err_t err = esp_tinylm_load(nlu_bin_start, &s_lm);
    if (err != ESP_OK) {
        /* Scratch and the KV cache are allocated in PSRAM; a part without it
         * gets ESP_ERR_NO_MEM here. Degrade rather than abort — the rest of the
         * product is still a working device. */
        ESP_LOGE(TAG, "model load failed (%s) — language commands disabled",
                 esp_err_to_name(err));
        s_lm = NULL;
        return err;
    }

    esp_tinylm_info_t info;
    if (esp_tinylm_get_info(s_lm, &info) == ESP_OK) {
        ESP_LOGI(TAG, "command model ready — vocab %d, dim %d, %d layers, context %d",
                 info.vocab_size, info.dim, info.n_layers, info.seq_len);
    }
#endif /* CONFIG_ZC_LM_MODEL_NLU */

#if CONFIG_ZC_LM_MODEL_STORY
    if (zc_lm_story_init() != ESP_OK) {
        ESP_LOGE(TAG, "story model load failed — generation disabled");
    }
#endif

    s_queue = xQueueCreate(1, sizeof(lm_job_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "queue alloc failed");
        return ESP_ERR_NO_MEM;
    }
    /* Core 1 for the PIE unit; 6 KB because the decode loop keeps the candidate
     * filter and two byte buffers on the stack. */
    if (xTaskCreatePinnedToCore(lm_task, "app_lm", 6144, NULL, 5, NULL, 1) != pdPASS) {
        ESP_LOGE(TAG, "worker task create failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t app_lm_submit_story(const char *prompt, int max_new, float temp,
                              app_lm_text_cb_t cb, void *arg)
{
#if !CONFIG_ZC_LM_MODEL_STORY
    (void)prompt; (void)max_new; (void)temp; (void)cb; (void)arg;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (prompt == NULL || cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lm_job_t job = { .kind = LM_JOB_STORY, .max_new = max_new, .temp = temp,
                     .cb = cb, .arg = arg };
    strlcpy(job.text, prompt, sizeof(job.text));
    if (xQueueSend(s_queue, &job, 0) != pdTRUE) {
        ESP_LOGW(TAG, "busy — dropped story prompt");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
#endif
}

esp_err_t app_lm_register(const char *device, const char *location,
                          app_lm_cb_t cb, void *arg)
{
    if (device == NULL || location == NULL || cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_nlisteners >= CONFIG_APP_LM_MAX_LISTENERS) {
        ESP_LOGE(TAG, "listener table full (%d) — raise CONFIG_APP_LM_MAX_LISTENERS",
                 CONFIG_APP_LM_MAX_LISTENERS);
        return ESP_ERR_NO_MEM;
    }
    s_listeners[s_nlisteners++] = (lm_listener_t){ device, location, cb, arg };
    return ESP_OK;
}

esp_err_t app_lm_submit(const char *cmd)
{
    if (s_queue == NULL || s_lm == NULL) {
        return ESP_ERR_INVALID_STATE;   /* no worker, or no command model */
    }
    if (cmd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    lm_job_t job = { .kind = LM_JOB_PARSE };
    strlcpy(job.text, cmd, sizeof(job.text));
    /* Do not block a console or event task waiting for an inference to finish;
     * one command in flight is the design, a second is dropped loudly. */
    if (xQueueSend(s_queue, &job, 0) != pdTRUE) {
        ESP_LOGW(TAG, "busy — dropped \"%s\"", cmd);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
