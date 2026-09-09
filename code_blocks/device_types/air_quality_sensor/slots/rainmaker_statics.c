static esp_rmaker_device_t *s_{{prefix_lc}}_device = NULL;
static esp_rmaker_param_t *s_{{prefix_lc}}_aqi_param = NULL;
{{#if cfg.co2_ppm_param}}
static esp_rmaker_param_t *s_{{prefix_lc}}_co2_param = NULL;
{{/if}}
{{#if cfg.pm25_param}}
static esp_rmaker_param_t *s_{{prefix_lc}}_pm25_param = NULL;
{{/if}}
{{#if cfg.tvoc_param}}
static esp_rmaker_param_t *s_{{prefix_lc}}_tvoc_param = NULL;
{{/if}}
