#define {{prefix}}_SMOOTH_TICK_MS  {{cfg.tick_ms}}
#define {{prefix}}_SMOOTH_STEP     {{cfg.step_per_tick}}
/* A step outside 1-254 never reaches the target (0) or overshoots the u8: clamp. */
#define {{prefix}}_SMOOTH_STEP_OK (({{prefix}}_SMOOTH_STEP) < 1 ? 1 : (({{prefix}}_SMOOTH_STEP) > 254 ? 254 : ({{prefix}}_SMOOTH_STEP)))
