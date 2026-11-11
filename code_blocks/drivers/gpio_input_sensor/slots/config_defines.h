#define {{prefix}}_INPUT_GPIO         {{cfg.gpio}}
#define {{prefix}}_INPUT_ACTIVE_LEVEL {{cfg.active_level}}
#define {{prefix}}_INPUT_POLL_MS      {{cfg.poll_interval_ms}}
#define {{prefix}}_INPUT_DEBOUNCE_MS  {{cfg.debounce_ms}}
/* The pull choice as a token the preprocessor can test: the template engine
   only does truthiness, so the string param becomes a defined/undefined macro. */
#define {{prefix}}_INPUT_PULL_{{cfg.pull}} 1
