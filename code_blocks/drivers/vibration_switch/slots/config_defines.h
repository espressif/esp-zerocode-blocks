#define {{prefix}}_VIB_GPIO         {{cfg.gpio}}
#define {{prefix}}_VIB_ACTIVE_LEVEL {{cfg.active_level}}
#define {{prefix}}_VIB_WINDOW_MS    {{cfg.window_ms}}
#define {{prefix}}_VIB_GAP_US       {{cfg.min_pulse_gap_us}}
/* The pull choice as a token the preprocessor can test. */
#define {{prefix}}_VIB_PULL_{{cfg.pull}} 1
/* Windows held at the active level with no pulse before it is reported. */
#define {{prefix}}_VIB_STUCK_WINDOWS 5
