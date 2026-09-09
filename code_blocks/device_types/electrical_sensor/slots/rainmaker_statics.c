static esp_rmaker_device_t *s_{{prefix_lc}}_device = NULL;
static esp_rmaker_param_t *s_{{prefix_lc}}_power_w_param = NULL;
{{#if cfg.voltage_param}}
static esp_rmaker_param_t *s_{{prefix_lc}}_voltage_param = NULL;
{{/if}}
{{#if cfg.current_param}}
static esp_rmaker_param_t *s_{{prefix_lc}}_current_param = NULL;
{{/if}}
