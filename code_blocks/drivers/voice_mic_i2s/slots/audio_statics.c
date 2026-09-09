/* {{prefix_lc}}: I2S voice capture — raw PCM source for the AFE feed task. */

static i2s_chan_handle_t s_{{prefix_lc}}_vmic_rx = NULL;

/* Scratch for the 32->16 bit narrowing. Sized at AFE's feed chunk, which is
 * queried at runtime; 1024 frames is comfortably above it for 16 kHz mono. */
#define {{prefix}}_VMIC_MAX_CHUNK 1024
static int32_t s_{{prefix_lc}}_vmic_raw[{{prefix}}_VMIC_MAX_CHUNK];

/* Fixed-name contract declared in app_audio.h — see this block's block.yml.
 * Blocking read; returns the sample count actually produced. */
size_t zc_audio_mic_read(int16_t *dest, size_t samples)
{
    if (!s_{{prefix_lc}}_vmic_rx || dest == NULL) return 0;

    size_t done = 0;
    while (done < samples) {
        size_t want = samples - done;
        if (want > {{prefix}}_VMIC_MAX_CHUNK) want = {{prefix}}_VMIC_MAX_CHUNK;

        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(s_{{prefix_lc}}_vmic_rx, s_{{prefix_lc}}_vmic_raw,
                                         want * sizeof(int32_t), &bytes_read,
                                         portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "{{prefix_lc}}: I2S read failed: %s", esp_err_to_name(err));
            return done;
        }

        size_t got = bytes_read / sizeof(int32_t);
        /* 24-bit parts left-justify into the 32-bit slot: the top 16 bits are
         * the usable signal for both 24- and 32-bit mics. */
        for (size_t i = 0; i < got; i++) {
            dest[done + i] = (int16_t)(s_{{prefix_lc}}_vmic_raw[i] >> 16);
        }
        done += got;
        if (got == 0) break;
    }
    return done;
}
