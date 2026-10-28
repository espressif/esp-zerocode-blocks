static esp_rmaker_device_t *s_{{prefix_lc}}_device = NULL;
static esp_rmaker_param_t *s_{{prefix_lc}}_position_param = NULL;
/* 1 when the instance reports a measured position separately (current_param). */
#define {{prefix}}_WC_SPLIT (0{{#if cfg.current_param}} + 1{{/if}})
