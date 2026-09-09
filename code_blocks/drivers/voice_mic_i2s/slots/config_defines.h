#define {{prefix}}_VMIC_PORT       {{cfg.port}}
#define {{prefix}}_VMIC_BCLK_GPIO  {{cfg.bclk_gpio}}
#define {{prefix}}_VMIC_WS_GPIO    {{cfg.ws_gpio}}
#define {{prefix}}_VMIC_DIN_GPIO   {{cfg.din_gpio}}
#define {{prefix}}_VMIC_SLOT_RIGHT {{cfg.slot_right}}
/* AFE requires 16 kHz mono — not a tunable. */
#define {{prefix}}_VMIC_SAMPLE_RATE 16000
