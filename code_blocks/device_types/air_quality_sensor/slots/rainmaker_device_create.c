{
    s_{{prefix_lc}}_device = esp_rmaker_device_create(
        "{{cfg.label}}", ESP_RMAKER_DEVICE_OTHER, NULL);
    if (!s_{{prefix_lc}}_device) {
        ESP_LOGE(TAG, "Failed to create {{prefix_lc}} device");
        return ESP_FAIL;
    }
    /* Read-only sensor: no write callback, no writable params. */
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device,
        esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "{{cfg.label}}"));
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.air_quality_param}}, &init);
    /* AirQuality enum 0-6 (see encoding above) — custom read-only int. */
    s_{{prefix_lc}}_aqi_param = esp_rmaker_param_create(
        "Air Quality", NULL, esp_rmaker_int(init.u8), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_aqi_param);
    esp_rmaker_device_assign_primary_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_aqi_param);
    {{#if cfg.co2_ppm_param}}
    app_driver_get_param({{cfg.co2_ppm_param}}, &init);
    s_{{prefix_lc}}_co2_param = esp_rmaker_param_create(
        "CO2 (ppm)", NULL, esp_rmaker_int(init.u16), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_co2_param);
    {{/if}}
    {{#if cfg.pm25_param}}
    app_driver_get_param({{cfg.pm25_param}}, &init);
    s_{{prefix_lc}}_pm25_param = esp_rmaker_param_create(
        "PM2.5 (ug/m3)", NULL, esp_rmaker_int(init.u16), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_pm25_param);
    {{/if}}
    {{#if cfg.tvoc_param}}
    app_driver_get_param({{cfg.tvoc_param}}, &init);
    s_{{prefix_lc}}_tvoc_param = esp_rmaker_param_create(
        "TVOC", NULL, esp_rmaker_int(init.u16), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_tvoc_param);
    {{/if}}
    esp_rmaker_node_add_device(node, s_{{prefix_lc}}_device);
    ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker Air Quality Sensor device '{{cfg.label}}'");
}
