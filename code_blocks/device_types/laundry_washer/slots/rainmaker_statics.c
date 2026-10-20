static esp_rmaker_device_t *s_{{prefix_lc}}_device = NULL;
{{#if cfg.mode_param}}
static esp_rmaker_param_t *s_{{prefix_lc}}_mode_param = NULL;
{{/if}}
static esp_rmaker_param_t *s_{{prefix_lc}}_state_param = NULL;
