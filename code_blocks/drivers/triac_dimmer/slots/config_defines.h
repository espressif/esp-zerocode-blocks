#define {{prefix}}_TRIAC_ZCD_GPIO   {{cfg.zcd_gpio}}
#define {{prefix}}_TRIAC_GATE_GPIO  {{cfg.gate_gpio}}
#define {{prefix}}_TRIAC_HALF_US    ({{cfg.ac_frequency_hz}} == 60 ? 8333 : 10000)
