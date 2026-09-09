{
    s_{{prefix_lc}}_device = esp_rmaker_device_create(
        "{{cfg.label}}", ESP_RMAKER_DEVICE_THERMOSTAT, NULL);
    if (!s_{{prefix_lc}}_device) {
        ESP_LOGE(TAG, "Failed to create {{prefix_lc}} device");
        return ESP_FAIL;
    }
    esp_rmaker_device_add_cb(s_{{prefix_lc}}_device, rainmaker_write_cb, NULL);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device,
        esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "{{cfg.label}}"));
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.local_temp_param}}, &init);
    s_{{prefix_lc}}_temp_param = esp_rmaker_temperature_param_create(
        ESP_RMAKER_DEF_TEMPERATURE_NAME, init.i16 / 100.0f);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_temp_param);
    app_driver_get_param({{cfg.heat_setpoint_param}}, &init);
    s_{{prefix_lc}}_setpoint_param = esp_rmaker_param_create(
        "Setpoint", ESP_RMAKER_PARAM_TEMPERATURE,
        esp_rmaker_float(init.i16 / 100.0f), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (s_{{prefix_lc}}_setpoint_param) {
        esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_setpoint_param, ESP_RMAKER_UI_SLIDER);
        esp_rmaker_param_add_bounds(s_{{prefix_lc}}_setpoint_param,
            esp_rmaker_float(7.0f), esp_rmaker_float(30.0f), esp_rmaker_float(0.5f));
    }
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_setpoint_param);
    app_driver_get_param({{cfg.system_mode_param}}, &init);
    s_{{prefix_lc}}_mode_param = esp_rmaker_param_create(
        "System Mode", ESP_RMAKER_PARAM_MODE,
        esp_rmaker_int(init.u8), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (s_{{prefix_lc}}_mode_param) {
        esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_mode_param, ESP_RMAKER_UI_DROPDOWN);
        esp_rmaker_param_add_bounds(s_{{prefix_lc}}_mode_param,
            esp_rmaker_int(0), esp_rmaker_int(4), esp_rmaker_int(1));
    }
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_mode_param);
    esp_rmaker_device_assign_primary_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_setpoint_param);
    esp_rmaker_node_add_device(node, s_{{prefix_lc}}_device);
    ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker Thermostat device '{{cfg.label}}'");
}
