#define {{prefix}}_FLT_SAMPLES   ({{cfg.samples}})
#define {{prefix}}_FLT_PERIOD_MS ({{cfg.sample_ms}})
#define {{prefix}}_FLT_STALE_MS  ({{cfg.stale_after_ms}})
/* mode and value kind as tokens the preprocessor can test */
#define {{prefix}}_FLT_MODE_{{cfg.mode}} 1
#define {{prefix}}_FLT_KIND_{{cfg.value_kind}} 1
