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
    app_driver_get_param({{cfg.active_power_param}}, &init);
    /* No standard power-measurement param type — custom read-only float in W. */
    s_{{prefix_lc}}_power_w_param = esp_rmaker_param_create(
        "Power", NULL, esp_rmaker_float(init.u32 / 1000.0f), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_power_w_param);
    esp_rmaker_device_assign_primary_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_power_w_param);
    {{#if cfg.voltage_param}}
    app_driver_get_param({{cfg.voltage_param}}, &init);
    s_{{prefix_lc}}_voltage_param = esp_rmaker_param_create(
        "Voltage", NULL, esp_rmaker_float(init.u32 / 1000.0f), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_voltage_param);
    {{/if}}
    {{#if cfg.current_param}}
    app_driver_get_param({{cfg.current_param}}, &init);
    s_{{prefix_lc}}_current_param = esp_rmaker_param_create(
        "Current", NULL, esp_rmaker_float(init.u32 / 1000.0f), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_current_param);
    {{/if}}
    esp_rmaker_node_add_device(node, s_{{prefix_lc}}_device);
    ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker Electrical Sensor device '{{cfg.label}}'");
}
