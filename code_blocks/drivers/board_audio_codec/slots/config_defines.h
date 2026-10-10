#define {{prefix}}_ACODEC_DEVICE      "{{cfg.device}}"
#define {{prefix}}_ACODEC_RATE        {{cfg.sample_rate}}
#define {{prefix}}_ACODEC_CHANNELS    {{cfg.channels}}
#define {{prefix}}_ACODEC_CODEC_VOL   {{cfg.codec_volume}}
/* Frames per esp_codec_dev_write: ~16 ms at 16 kHz, ~5 ms at 48 kHz — bounds
 * how fast a tone/mute change takes effect. */
#define {{prefix}}_ACODEC_CHUNK       256
