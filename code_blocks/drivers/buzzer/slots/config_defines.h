#define {{prefix}}_BUZZER_GPIO     {{cfg.gpio}}
#define {{prefix}}_BUZZER_CHANNEL  {{cfg.ledc_channel}}
#define {{prefix}}_BUZZER_TIMER    {{cfg.ledc_timer}}
/* kind: passive -> LEDC tone; active -> plain GPIO level (see block.yml). */
#define {{prefix}}_BUZZER_KIND_{{cfg.kind}} 1
#define {{prefix}}_BUZZER_ACTIVE_LEVEL {{cfg.active_level}}
/* Tone duty resolution; 50 % duty is derived from it, never hard-coded. */
#define {{prefix}}_BUZZER_DUTY_BITS 8
