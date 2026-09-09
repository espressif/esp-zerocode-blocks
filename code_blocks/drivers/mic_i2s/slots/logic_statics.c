/* {{prefix_lc}}: I2S microphone — RMS loudness sampler. */

static i2s_chan_handle_t s_{{prefix_lc}}_rx = NULL;

/* One window of raw 32-bit slots. Static, not on the task stack: at the
 * default 1024 samples this is 4 KB, which would blow a normal task stack. */
static int32_t s_{{prefix_lc}}_raw[{{prefix}}_MIC_WINDOW];

/* Integer Newton sqrt — avoids pulling libm into app_logic for one call. */
static uint32_t {{prefix_lc}}_isqrt(uint64_t v)
{
    if (v == 0) return 0;
    uint64_t x = v, y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + v / x) / 2;
    }
    return (uint32_t)x;
}

static void {{prefix_lc}}_mic_task(void *arg)
{
    while (1) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(s_{{prefix_lc}}_rx, s_{{prefix_lc}}_raw,
                                         sizeof(s_{{prefix_lc}}_raw), &bytes_read,
                                         portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "{{prefix_lc}}: I2S read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int n = (int)(bytes_read / sizeof(int32_t));
        if (n <= 0) continue;

        /* 24-bit parts left-justify into the 32-bit slot, so the top 16 bits
         * are the usable signal regardless of whether the mic is 24- or
         * 32-bit. */
        int64_t sum = 0;
        for (int i = 0; i < n; i++) {
            sum += (int16_t)(s_{{prefix_lc}}_raw[i] >> 16);
        }
        int32_t mean = (int32_t)(sum / n);

        /* MEMS mics carry a DC bias; RMS about the raw zero would report that
         * bias as permanent loudness. Measure about the window mean instead. */
        uint64_t sq = 0;
        for (int i = 0; i < n; i++) {
            int32_t s = (int32_t)(int16_t)(s_{{prefix_lc}}_raw[i] >> 16) - mean;
            sq += (uint64_t)((int64_t)s * (int64_t)s);
        }
        uint32_t rms = {{prefix_lc}}_isqrt(sq / (uint64_t)n);

        /* rms spans 0..32767; report across the full u16 range. */
        uint32_t level = rms * 2;
        if (level > 65535) level = 65535;

        app_driver_param_val_t v = { .u16 = (uint16_t)level };
        app_driver_set_param({{cfg.level_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    }
}
