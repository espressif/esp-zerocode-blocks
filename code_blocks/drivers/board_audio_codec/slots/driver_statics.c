/* {{prefix_lc}}: the board's own '{{cfg.device}}' audio codec — the same
 * square-wave generator as drivers/speaker_es8311, writing into the codec
 * device esp-board-manager already created. No pin, port or address appears
 * in this product: they are the board's. */

static esp_codec_dev_handle_t s_{{prefix_lc}}_codec = NULL;
static int s_{{prefix_lc}}_rate = 16000;

/* Written from driver_apply_cases (any task), read by the audio task.
 * Scalar, aligned, single-writer-per-field — volatile is sufficient; a lock
 * here would risk blocking the param bus behind a codec write. A change takes
 * effect on the next chunk boundary. */
static volatile uint32_t s_{{prefix_lc}}_tone_hz = 0;
static volatile uint8_t  s_{{prefix_lc}}_volume  = 50;
static volatile bool     s_{{prefix_lc}}_muted   = false;

static void {{prefix_lc}}_acodec_task(void *arg)
{
    static int16_t {{prefix_lc}}_buf[{{prefix}}_ACODEC_CHUNK * {{prefix}}_ACODEC_CHANNELS];
    /* Q16 phase: low 16 bits are the fraction of one cycle. */
    uint32_t phase = 0;

    while (1) {
        uint32_t hz  = s_{{prefix_lc}}_tone_hz;
        uint8_t  vol = s_{{prefix_lc}}_volume;
        bool     mut = s_{{prefix_lc}}_muted;

        if (hz == 0 || mut || vol == 0) {
            memset({{prefix_lc}}_buf, 0, sizeof({{prefix_lc}}_buf));
            phase = 0;
        } else {
            if (vol > 100) vol = 100;
            /* 12000 of int16 full scale leaves headroom so a class-D amp
             * doesn't clip on the square wave's edges. */
            int16_t amp = (int16_t)((12000 * (int32_t)vol) / 100);
            uint32_t step = (uint32_t)(((uint64_t)hz << 16) / (uint32_t)s_{{prefix_lc}}_rate);
            for (int i = 0; i < {{prefix}}_ACODEC_CHUNK; i++) {
                int16_t s = (phase & 0x8000u) ? amp : (int16_t)(-amp);
                for (int c = 0; c < {{prefix}}_ACODEC_CHANNELS; c++) {
                    {{prefix_lc}}_buf[i * {{prefix}}_ACODEC_CHANNELS + c] = s;
                }
                phase = (phase + step) & 0xFFFFu;
            }
        }
        /* Blocks until the codec's DMA takes the chunk — this paces the loop. */
        if (esp_codec_dev_write(s_{{prefix_lc}}_codec, {{prefix_lc}}_buf, (int)sizeof({{prefix_lc}}_buf)) != ESP_CODEC_DEV_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));   /* never spin on a failing codec */
        }
    }
}

/* The MCLK ratio and sample rate the board's I2S peripheral is configured
 * for. esp_codec_dev 2.x needs the ratio at open time; a mismatch leaves the
 * codec silent with no error anywhere. 0 = the board states none. */
static void {{prefix_lc}}_board_clock(int *rate, int *mclk_multiple)
{
    *rate = 0;
    *mclk_multiple = 0;
    dev_audio_codec_config_t *dcfg = NULL;
    if (esp_board_manager_get_device_config({{prefix}}_ACODEC_DEVICE, (void **)&dcfg) != ESP_OK ||
        dcfg == NULL || dcfg->i2s_cfg.name == NULL) {
        return;
    }
    periph_i2s_config_t *pcfg = NULL;
    if (esp_board_manager_get_periph_config(dcfg->i2s_cfg.name, (void **)&pcfg) != ESP_OK || pcfg == NULL) {
        return;
    }
    if (pcfg->mode == I2S_COMM_MODE_STD) {
        *rate = (int)pcfg->i2s_cfg.std.clk_cfg.sample_rate_hz;
        *mclk_multiple = (int)pcfg->i2s_cfg.std.clk_cfg.mclk_multiple;
    }
#if CONFIG_SOC_I2S_SUPPORTS_TDM
    else if (pcfg->mode == I2S_COMM_MODE_TDM) {
        *rate = (int)pcfg->i2s_cfg.tdm.clk_cfg.sample_rate_hz;
        *mclk_multiple = (int)pcfg->i2s_cfg.tdm.clk_cfg.mclk_multiple;
    }
#endif
}
