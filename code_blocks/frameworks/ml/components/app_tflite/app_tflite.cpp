/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI — TensorFlow Lite Micro framework
 *
 * Self-contained: the generator copies this component verbatim and never
 * rewrites it (only frameworks whose device types contribute slot code get
 * generated implementations). Everything product-specific arrives through
 * zc_tflite_session_cfg_t from a driver block.
 */

#include "app_tflite.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <new>

static const char *TAG = "app_tflite";

struct zc_tflite_session {
    const tflite::Model *model;
    tflite::MicroInterpreter *interpreter;
    uint8_t *arena;
    TfLiteTensor *input;
    TfLiteTensor *output;
    int64_t last_us;
};

/**
 * Operator sets. Each is a function-local static so the resolver outlives
 * every interpreter built from it, and is registered exactly once even if a
 * product creates several sessions with the same set.
 *
 * The CNN set is deliberately the seven kernels esp-nn accelerates (add,
 * conv, depthwise_conv, fully_connected, mul, pooling, softmax) plus the
 * reshape/quantize plumbing a converted graph always carries.
 */
static const tflite::MicroOpResolver *resolver_for(zc_tflite_ops_t ops)
{
    switch (ops) {
    case ZC_TFLITE_OPS_MINIMAL: {
        static tflite::MicroMutableOpResolver<1> r;
        static bool done = false;
        if (!done) {
            r.AddFullyConnected();
            done = true;
        }
        return &r;
    }
    case ZC_TFLITE_OPS_DNN: {
        static tflite::MicroMutableOpResolver<5> r;
        static bool done = false;
        if (!done) {
            r.AddFullyConnected();
            r.AddSoftmax();
            r.AddReshape();
            r.AddQuantize();
            r.AddDequantize();
            done = true;
        }
        return &r;
    }
    case ZC_TFLITE_OPS_CNN: {
        static tflite::MicroMutableOpResolver<12> r;
        static bool done = false;
        if (!done) {
            r.AddConv2D();
            r.AddDepthwiseConv2D();
            r.AddAveragePool2D();
            r.AddMaxPool2D();
            r.AddFullyConnected();
            r.AddSoftmax();
            r.AddReshape();
            r.AddQuantize();
            r.AddDequantize();
            r.AddAdd();
            r.AddMul();
            r.AddLogistic();
            done = true;
        }
        return &r;
    }
    }
    ESP_LOGE(TAG, "unknown op set %d", (int)ops);
    return nullptr;
}

esp_err_t app_tflite_init(void)
{
#if defined(CONFIG_NN_OPTIMIZED)
    ESP_LOGI(TAG, "TFLite Micro ready — esp-nn optimized kernels enabled");
#else
    ESP_LOGW(TAG, "TFLite Micro ready — esp-nn optimizations OFF, inference "
                  "will be several times slower");
#endif
    return ESP_OK;
}

zc_tflite_session_t *zc_tflite_session_create(const zc_tflite_session_cfg_t *cfg)
{
    if (cfg == nullptr || cfg->model_data == nullptr || cfg->arena_bytes == 0) {
        ESP_LOGE(TAG, "session_create: bad config");
        return nullptr;
    }

    /* PREPARE 1/4 — map the flatbuffer. No copy, no parse; it does require
     * the buffer to be 8-byte aligned, which is why the model arrays carry
     * an explicit alignas(8). */
    const tflite::Model *model = tflite::GetModel(cfg->model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "model schema %lu != supported %d — reconvert the model",
                 (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
        return nullptr;
    }

    zc_tflite_session_t *s = (zc_tflite_session_t *)calloc(1, sizeof(*s));
    if (s == nullptr) {
        ESP_LOGE(TAG, "session alloc failed");
        return nullptr;
    }
    s->model = model;

    /* PREPARE 2/4 — the tensor arena. heap_caps_malloc_prefer falls back to
     * internal RAM when PSRAM is asked for but absent, so a PSRAM-preferring
     * product still boots on a board without it. */
    if (cfg->arena_in_psram) {
        s->arena = (uint8_t *)heap_caps_malloc_prefer(cfg->arena_bytes, 2,
                                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    } else {
        s->arena = (uint8_t *)heap_caps_malloc(cfg->arena_bytes,
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (s->arena == nullptr) {
        ESP_LOGE(TAG, "tensor arena of %u bytes failed to allocate",
                 (unsigned)cfg->arena_bytes);
        free(s);
        return nullptr;
    }

    /* PREPARE 3/4 — operator set + interpreter. */
    const tflite::MicroOpResolver *resolver = resolver_for(cfg->ops);
    if (resolver == nullptr) {
        heap_caps_free(s->arena);
        free(s);
        return nullptr;
    }

    s->interpreter = new (std::nothrow)
        tflite::MicroInterpreter(model, *resolver, s->arena, cfg->arena_bytes);
    if (s->interpreter == nullptr) {
        ESP_LOGE(TAG, "interpreter alloc failed");
        heap_caps_free(s->arena);
        free(s);
        return nullptr;
    }

    /* PREPARE 4/4 — carve the arena into tensors. This is where a too-small
     * arena, or an operator missing from the chosen set, actually fails. */
    if (s->interpreter->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors() failed — arena too small for this "
                      "model, or an operator is missing from the op set");
        delete s->interpreter;
        heap_caps_free(s->arena);
        free(s);
        return nullptr;
    }

    s->input = s->interpreter->input(0);
    s->output = s->interpreter->output(0);
    const unsigned used = (unsigned)s->interpreter->arena_used_bytes();
    ESP_LOGI(TAG, "session ready — arena %u B (%s), input %u B, arena used %u B",
             (unsigned)cfg->arena_bytes, cfg->arena_in_psram ? "psram-preferred" : "internal",
             (unsigned)s->input->bytes, used);

    /* The same numbers again, in a form a script can read. This is the ONLY
     * way to learn a model's real arena — TFLM computes it during
     * AllocateTensors and there is no offline equivalent — so it must survive
     * into a log a bench run can grep, not just a human's serial monitor.
     *
     * `want` is what to put in the manifest: TFLM's own guidance
     * (micro_interpreter.h) is that the optimal arena is arena_used_bytes()
     * plus 16, because the arena wants 16-byte alignment to be fully usable.
     *
     * Printed at WARN deliberately: a product built with no_app_logs still
     * emits it, and a bench run must not depend on the product's log level.
     * It is one line, once per session create. */
    ESP_LOGW(TAG, "ZC_ARENA model=%s configured=%u used=%u want=%u input=%u psram=%d",
             cfg->name ? cfg->name : "?",
             (unsigned)cfg->arena_bytes, used, used + 16,
             (unsigned)s->input->bytes, cfg->arena_in_psram ? 1 : 0);
    return s;
}

size_t zc_tflite_arena_used(zc_tflite_session_t *s)
{
    return (s && s->interpreter) ? s->interpreter->arena_used_bytes() : 0;
}

esp_err_t zc_tflite_invoke(zc_tflite_session_t *s)
{
    if (s == nullptr || s->interpreter == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    const int64_t t0 = esp_timer_get_time();
    const TfLiteStatus st = s->interpreter->Invoke();
    s->last_us = esp_timer_get_time() - t0;
    if (st != kTfLiteOk) {
        ESP_LOGE(TAG, "Invoke() failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void *zc_tflite_input(zc_tflite_session_t *s)
{
    return (s && s->input) ? (void *)s->input->data.data : nullptr;
}

size_t zc_tflite_input_bytes(zc_tflite_session_t *s)
{
    return (s && s->input) ? s->input->bytes : 0;
}

/** Bytes per element of a tensor's type — the tensor carries `bytes`, not a
 *  count, so element access needs the divisor. */
static size_t elem_size(const TfLiteTensor *t)
{
    switch (t->type) {
    case kTfLiteInt8:
    case kTfLiteUInt8:
        return 1;
    case kTfLiteInt16:
        return 2;
    case kTfLiteFloat32:
    case kTfLiteInt32:
        return 4;
    default:
        return 0;
    }
}

size_t zc_tflite_output_count(zc_tflite_session_t *s)
{
    if (s == nullptr || s->output == nullptr) {
        return 0;
    }
    const size_t es = elem_size(s->output);
    return es ? s->output->bytes / es : 0;
}

float zc_tflite_output_value(zc_tflite_session_t *s, size_t i)
{
    if (i >= zc_tflite_output_count(s)) {
        return 0.0f;
    }
    const TfLiteTensor *o = s->output;
    /* Dequantize to the real value the model was trained on. A float output
     * carries no scale/zero_point and is already real. */
    switch (o->type) {
    case kTfLiteInt8:
        return (o->data.int8[i] - o->params.zero_point) * o->params.scale;
    case kTfLiteUInt8:
        return (o->data.uint8[i] - o->params.zero_point) * o->params.scale;
    case kTfLiteFloat32:
        return o->data.f[i];
    default:
        ESP_LOGW(TAG, "output type %d not handled", (int)o->type);
        return 0.0f;
    }
}

size_t zc_tflite_output_argmax(zc_tflite_session_t *s)
{
    const size_t n = zc_tflite_output_count(s);
    size_t best = 0;
    float best_v = 0.0f;
    for (size_t i = 0; i < n; i++) {
        const float v = zc_tflite_output_value(s, i);
        if (i == 0 || v > best_v) {
            best_v = v;
            best = i;
        }
    }
    return best;
}

int64_t zc_tflite_last_invoke_us(zc_tflite_session_t *s)
{
    return s ? s->last_us : 0;
}
