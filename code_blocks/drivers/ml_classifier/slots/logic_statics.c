/* ── {{prefix}}: TFLite Micro classifier ─────────────────────────────── */

extern const unsigned char {{cfg.model_symbol}}[];

static zc_tflite_session_t *s_{{prefix_lc}}_session;

/**
 * Input feed. WEAK on purpose: with no override this zeroes the tensor, so
 * the product builds and the loop runs end to end without a sensor. Define a
 * strong {{prefix_lc}}_fill_input() in your own translation unit — a camera
 * frame, a mic window, an accelerometer buffer — and the linker prefers it.
 *
 * `in` points at the model's input tensor and `len` is its size in BYTES,
 * not elements. For a standard INT8 graph, cast to int8_t *.
 */
__attribute__((weak)) void {{prefix_lc}}_fill_input(void *in, size_t len)
{
    memset(in, 0, len);
}

/* A TASK, not an esp_timer callback like the sensor drivers use: an inference
 * runs for tens to hundreds of milliseconds (426 ms for a person-detection
 * CNN on esp32c3), and that much work inside the shared esp_timer task would
 * stall every other timer in the product. */
static void {{prefix_lc}}_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_TFL_INTERVAL_MS));

        /* INVOKE — feed, run, time. */
        {{prefix_lc}}_fill_input(zc_tflite_input(s_{{prefix_lc}}_session),
                                 zc_tflite_input_bytes(s_{{prefix_lc}}_session));
        if (zc_tflite_invoke(s_{{prefix_lc}}_session) != ESP_OK) {
            continue;
        }

        /* POST-PROCESS — argmax, dequantize, clamp to a percentage. */
        size_t top = zc_tflite_output_argmax(s_{{prefix_lc}}_session);
        float score = zc_tflite_output_value(s_{{prefix_lc}}_session, top);
        int pct = (int)(score * 100.0f + 0.5f);
        if (pct < 0) {
            pct = 0;
        } else if (pct > 100) {
            pct = 100;
        }

        ESP_LOGD(TAG, "{{prefix_lc}}: class %u score %d%% in %lld us",
                 (unsigned)top, pct,
                 (long long)zc_tflite_last_invoke_us(s_{{prefix_lc}}_session));

        /* STATE, not events — a repeated identical verdict is deliberately
         * not re-propagated. See this block's block.yml. */
        app_driver_param_val_t {{prefix_lc}}_det = { .b = (pct >= {{prefix}}_TFL_THRESHOLD_PCT) };
        app_driver_set_param({{cfg.detect_param}}, {{prefix_lc}}_det, APP_DRIVER_SOURCE_LOCAL);
{{#if cfg.score_param}}
        app_driver_param_val_t {{prefix_lc}}_sc = { .u8 = (uint8_t)pct };
        app_driver_set_param({{cfg.score_param}}, {{prefix_lc}}_sc, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
    }
}
