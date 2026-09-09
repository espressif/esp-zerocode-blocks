/* {{prefix_lc}}: I2S speaker — square-wave tone generator on a TX channel. */

static i2s_chan_handle_t s_{{prefix_lc}}_tx = NULL;

/* Written from driver_apply_cases (any task), read by the audio task.
 * Scalar, aligned, single-writer-per-field — volatile is sufficient; a lock
 * here would risk blocking the param bus behind a 16 ms I2S write. A change
 * takes effect on the next chunk boundary. */
static volatile uint32_t s_{{prefix_lc}}_tone_hz = 0;
static volatile uint8_t  s_{{prefix_lc}}_volume  = 50;
static volatile bool     s_{{prefix_lc}}_muted   = false;

static void {{prefix_lc}}_spk_task(void *arg)
{
    static int16_t {{prefix_lc}}_buf[{{prefix}}_SPK_CHUNK];
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
            /* 12000 of int16 full scale leaves headroom so a cheap class-D
             * amp doesn't clip on the square wave's edges. */
            int16_t amp = (int16_t)((12000 * (int32_t)vol) / 100);
            uint32_t step = (uint32_t)(((uint64_t)hz << 16) / {{prefix}}_SPK_SAMPLE_RATE);
            for (int i = 0; i < {{prefix}}_SPK_CHUNK; i++) {
                {{prefix_lc}}_buf[i] = (phase & 0x8000u) ? amp : (int16_t)(-amp);
                phase = (phase + step) & 0xFFFFu;
            }
        }

        size_t written = 0;
        /* Blocks until DMA drains — this is what paces the loop. */
        i2s_channel_write(s_{{prefix_lc}}_tx, {{prefix_lc}}_buf,
                          sizeof({{prefix_lc}}_buf), &written, portMAX_DELAY);
    }
}
