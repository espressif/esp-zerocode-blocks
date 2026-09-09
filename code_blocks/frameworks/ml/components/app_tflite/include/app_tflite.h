/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI — TensorFlow Lite Micro framework
 *
 * A thin, reusable wrapper around the shape every esp-tflite-micro example
 * repeats: PREPARE once, INVOKE per sample, POST-PROCESS the output tensor.
 * Driver blocks own the model, the input feed and the param bus; this
 * component owns the interpreter, the tensor arena and the operator set.
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Which operators to register with the interpreter.
 *
 * TFLite Micro resolves operators at COMPILE time — there is no
 * AllOpsResolver — so a model can only run if its every operator is in the
 * set named here. Registering more than a model needs costs flash; a model
 * whose operator is missing fails AllocateTensors() with a clear log line.
 * Need one that is absent? Add it to resolver_for() below, in an MR.
 */
typedef enum {
    /** FullyConnected only. The reference model; smallest possible binary. */
    ZC_TFLITE_OPS_MINIMAL = 0,
    /** Dense classifiers: FullyConnected, Softmax, Reshape, Quantize, Dequantize. */
    ZC_TFLITE_OPS_DNN = 1,
    /** Quantized CNNs (person detection, keyword spotting, small MobileNets).
     *  Covers all seven kernels esp-nn accelerates. */
    ZC_TFLITE_OPS_CNN = 2,
} zc_tflite_ops_t;

/**
 * Lowercase aliases of the constants above.
 *
 * Slot templating substitutes text and has no case conversion, so a block
 * that wants to turn a product's `ops_profile: cnn` into an enum constant
 * needs a lowercase spelling to paste into. A typo in product.yml then
 * becomes a compile error naming the identifier, not a silent fallback.
 */
#define ZC_TFLITE_OPS_ALIAS_minimal ZC_TFLITE_OPS_MINIMAL
#define ZC_TFLITE_OPS_ALIAS_dnn     ZC_TFLITE_OPS_DNN
#define ZC_TFLITE_OPS_ALIAS_cnn     ZC_TFLITE_OPS_CNN

typedef struct zc_tflite_session zc_tflite_session_t;

typedef struct {
    /** Pointer to the .tflite flatbuffer. MUST be 8-byte aligned — a C array
     *  produced by `xxd -i` needs an explicit alignas(8), as the examples do.
     *  Pass zc_tflite_reference_model to run the shipped sine model. */
    const void *model_data;
    /** Tensor arena size. Too small fails AllocateTensors() and the log names
     *  the shortfall; there is no way to compute it without running. */
    size_t arena_bytes;
    /** Prefer PSRAM for the arena, falling back to internal RAM. Internal is
     *  measurably faster (47 ms vs 54 ms on s3 person detection) but scarce. */
    bool arena_in_psram;
    zc_tflite_ops_t ops;
    /** Model id for logs — the manifest name in models/<id>.yml. Optional;
     *  "?" when NULL. It exists so a bench run can tell which model a
     *  ZC_ARENA line belongs to when a product runs more than one. */
    const char *name;
} zc_tflite_session_cfg_t;

/** Framework init. Called once from app_main; logs the build's esp-nn state. */
esp_err_t app_tflite_init(void);

/* ── PREPARE ─────────────────────────────────────────────────────────── */

/** Build an interpreter for one model. NULL on failure (every failure path
 *  logs its reason). Sessions are long-lived — create once, invoke forever. */
zc_tflite_session_t *zc_tflite_session_create(const zc_tflite_session_cfg_t *cfg);

/* ── INVOKE ──────────────────────────────────────────────────────────── */

/** Run the model over whatever is currently in the input tensor. */
esp_err_t zc_tflite_invoke(zc_tflite_session_t *s);

/** Input tensor, to be filled BEFORE zc_tflite_invoke(). The element type is
 *  the model's — int8_t for a standard INT8-quantized graph. */
void *zc_tflite_input(zc_tflite_session_t *s);
size_t zc_tflite_input_bytes(zc_tflite_session_t *s);

/* ── POST-PROCESS ────────────────────────────────────────────────────── */

/** Number of elements in output tensor 0. */
size_t zc_tflite_output_count(zc_tflite_session_t *s);

/** Output element `i`, dequantized to its real value:
 *  (raw - zero_point) * scale. Handles int8 / uint8 / float32 outputs.
 *  Returns 0.0f if `i` is out of range. */
float zc_tflite_output_value(zc_tflite_session_t *s, size_t i);

/** Index of the largest output element — argmax for a classifier. */
size_t zc_tflite_output_argmax(zc_tflite_session_t *s);

/** Wall time of the last zc_tflite_invoke(), microseconds. */
int64_t zc_tflite_last_invoke_us(zc_tflite_session_t *s);

/** Arena the model ACTUALLY needed, in bytes — TFLM computes this during
 *  AllocateTensors and there is no offline equivalent, so a model's arena can
 *  only be learned by running it.
 *
 *  Size the manifest at this + 16: the arena wants 16-byte alignment to be
 *  fully usable, which is TFLM's own stated rule (micro_interpreter.h).
 *
 *  session_create already prints this as a greppable `ZC_ARENA` line, which is
 *  what scripts/bench-arena.sh reads; this accessor is for code that wants to
 *  act on it. Returns 0 before AllocateTensors or on a NULL session. */
size_t zc_tflite_arena_used(zc_tflite_session_t *s);

/** Reference model: the TFLM "hello world" sine regressor, 2488 bytes, one
 *  FullyConnected graph. Present so this framework LINKS and runs with no
 *  product-supplied model — a smoke test for CI and ml-inference-demo, not a
 *  useful inference. Its output means nothing.
 *
 *  A real product points its block's model_symbol at a different array; this
 *  one is then unreferenced and --gc-sections drops it from the binary, so
 *  shipping costs nothing. See frameworks/ml/block.yml ("BRINGING A REAL
 *  MODEL") for the three ways a model gets compiled into a build — none of
 *  which is "add it to a driver block", which cannot carry files. */
extern const unsigned char zc_tflite_reference_model[];

#ifdef __cplusplus
}
#endif
