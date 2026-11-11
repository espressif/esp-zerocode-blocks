#define {{prefix}}_UP_GPIO          {{cfg.up_gpio}}
#define {{prefix}}_DOWN_GPIO        {{cfg.down_gpio}}
#define {{prefix}}_UPPER_LIMIT_GPIO {{cfg.upper_limit_gpio}}
#define {{prefix}}_LOWER_LIMIT_GPIO {{cfg.lower_limit_gpio}}
#define {{prefix}}_LIMIT_ACTIVE     {{cfg.limit_active_level}}
#define {{prefix}}_MAX_RUN_MS       {{cfg.max_run_ms}}
#define {{prefix}}_HAS_CURRENT      (0{{#if cfg.current_param}} + 1{{/if}})
