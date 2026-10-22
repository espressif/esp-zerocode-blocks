#define {{prefix}}_INTR_GPIO         {{cfg.gpio}}
#define {{prefix}}_INTR_ACTIVE_LEVEL {{cfg.active_level}}
/* The pull choice as a token the preprocessor can test: the template engine
   only does truthiness, so the string param becomes a defined/undefined macro. */
#define {{prefix}}_INTR_PULL_{{cfg.pull}} 1
#define {{prefix}}_INTR_DEBOUNCE_MS {{cfg.debounce_ms}}
