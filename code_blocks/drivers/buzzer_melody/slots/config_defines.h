#define {{prefix}}_MEL_GPIO     {{cfg.gpio}}
#define {{prefix}}_MEL_CHANNEL  {{cfg.ledc_channel}}
#define {{prefix}}_MEL_TIMER    {{cfg.ledc_timer}}
/* kind: passive -> LEDC tones; active -> plain GPIO level keyed in rhythm. */
#define {{prefix}}_MEL_KIND_{{cfg.kind}} 1
#define {{prefix}}_MEL_ACTIVE_LEVEL {{cfg.active_level}}
#define {{prefix}}_MEL_DUTY_BITS 8
