#define {{prefix}}_INTR_GPIO         {{cfg.gpio}}
#define {{prefix}}_INTR_ACTIVE_LEVEL {{cfg.active_level}}
/* The pull choice as a token the preprocessor can test: the template engine
   only does truthiness, so the string param becomes a defined/undefined macro. */
#define {{prefix}}_INTR_PULL_{{cfg.pull}} 1
#define {{prefix}}_INTR_DEBOUNCE_MS {{cfg.debounce_ms}}
/* The edge choice, the same way. A single edge makes the input momentary and
   names its press level; any-edge publishes the settled level as state. */
#define {{prefix}}_INTR_EDGE_{{cfg.intr_type}} 1
#if defined({{prefix}}_INTR_EDGE_GPIO_INTR_POSEDGE)
#define {{prefix}}_INTR_MOMENTARY   1
#define {{prefix}}_INTR_PRESS_LEVEL 1
#elif defined({{prefix}}_INTR_EDGE_GPIO_INTR_NEGEDGE)
#define {{prefix}}_INTR_MOMENTARY   1
#define {{prefix}}_INTR_PRESS_LEVEL 0
#else
#define {{prefix}}_INTR_MOMENTARY   0
#define {{prefix}}_INTR_PRESS_LEVEL {{prefix}}_INTR_ACTIVE_LEVEL
#endif
