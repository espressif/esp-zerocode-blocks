/* ── {{prefix}}: person detection (TFLite Micro + DVP camera) ─────────── */

/* The model itself lives in esp-tflite-micro (examples/person_detection) and is
 * compiled in by frameworks/ml's component CMakeLists — see this block's
 * block.yml for why it is referenced rather than vendored. */
extern const unsigned char g_person_detect_model_data[];

static zc_tflite_session_t *s_{{prefix_lc}}_session;

/* A TASK, not an esp_timer callback: this CNN invokes for ~400 ms on an esp32,
 * and that much work inside the shared timer task would stall every other
 * timer in the product. Same reasoning as drivers/ml_classifier. */
static void {{prefix_lc}}_task(void *arg)
{
    (void)arg;
    const size_t want = (size_t)({{prefix}}_PD_W * {{prefix}}_PD_H);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_PD_INTERVAL_MS));

        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == NULL) {
            ESP_LOGW(TAG, "{{prefix_lc}}: frame capture failed");
            continue;
        }

        int8_t *in = (int8_t *)zc_tflite_input(s_{{prefix_lc}}_session);
        size_t len = zc_tflite_input_bytes(s_{{prefix_lc}}_session);
        /* Guard both ends: a sensor that returned a different geometry than it
         * was configured for must not walk off either buffer. */
        size_t n = (fb->len < want) ? fb->len : want;
        if (n > len) {
            n = len;
        }
        /* uint8 [0,255] → int8 [-128,127]: the quantisation this graph was
         * trained with. Not a cast — a re-centring. */
        for (size_t i = 0; i < n; i++) {
            in[i] = (int8_t)((int)fb->buf[i] - 128);
        }
        if (n < len) {
            memset(in + n, 0, len - n);
        }
        esp_camera_fb_return(fb);

        if (zc_tflite_invoke(s_{{prefix_lc}}_session) != ESP_OK) {
            continue;
        }

        float score = zc_tflite_output_value(s_{{prefix_lc}}_session,
                                             {{prefix}}_PD_PERSON_INDEX);
        int pct = (int)(score * 100.0f + 0.5f);
        if (pct < 0) {
            pct = 0;
        } else if (pct > 100) {
            pct = 100;
        }

        ESP_LOGD(TAG, "{{prefix_lc}}: person %d%% in %lld us", pct,
                 (long long)zc_tflite_last_invoke_us(s_{{prefix_lc}}_session));

        app_driver_param_val_t {{prefix_lc}}_pres = { .b = (pct >= {{prefix}}_PD_THRESHOLD_PCT) };
        app_driver_set_param({{cfg.present_param}}, {{prefix_lc}}_pres, APP_DRIVER_SOURCE_LOCAL);
{{#if cfg.score_param}}
        app_driver_param_val_t {{prefix_lc}}_sc = { .u8 = (uint8_t)pct };
        app_driver_set_param({{cfg.score_param}}, {{prefix_lc}}_sc, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
    }
}
