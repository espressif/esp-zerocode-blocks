{
    s_{{prefix_lc}}_device = esp_rmaker_device_create(
        "{{cfg.label}}", ESP_RMAKER_DEVICE_OTHER, NULL);
    if (!s_{{prefix_lc}}_device) {
        ESP_LOGE(TAG, "Failed to create {{prefix_lc}} device");
        return ESP_FAIL;
    }
    esp_rmaker_device_add_cb(s_{{prefix_lc}}_device, rainmaker_write_cb, NULL);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device,
        esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "{{cfg.label}}"));
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.cook_time_param}}, &init);
    s_{{prefix_lc}}_cook_time_param = esp_rmaker_param_create(
        "Cook Time", NULL,
        esp_rmaker_int((int)init.u32), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (s_{{prefix_lc}}_cook_time_param) {
        esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_cook_time_param, ESP_RMAKER_UI_SLIDER);
        esp_rmaker_param_add_bounds(s_{{prefix_lc}}_cook_time_param,
            esp_rmaker_int(0), esp_rmaker_int(3600), esp_rmaker_int(30));
    }
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_cook_time_param);
    app_driver_get_param({{cfg.power_level_param}}, &init);
    s_{{prefix_lc}}_power_level_param = esp_rmaker_param_create(
        "Power Level", NULL,
        esp_rmaker_int(init.u8), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (s_{{prefix_lc}}_power_level_param) {
        esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_power_level_param, ESP_RMAKER_UI_SLIDER);
        esp_rmaker_param_add_bounds(s_{{prefix_lc}}_power_level_param,
            esp_rmaker_int(10), esp_rmaker_int(100), esp_rmaker_int(10));
    }
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_power_level_param);
    app_driver_get_param({{cfg.state_param}}, &init);
    s_{{prefix_lc}}_state_param = esp_rmaker_param_create(
        "Operational State", NULL,
        esp_rmaker_int(init.u8), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_state_param);
    esp_rmaker_device_assign_primary_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_cook_time_param);
    esp_rmaker_node_add_device(node, s_{{prefix_lc}}_device);
    ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker Microwave Oven device '{{cfg.label}}'");
}
