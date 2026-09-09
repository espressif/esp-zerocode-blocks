{
    s_{{prefix_lc}}_device = esp_rmaker_device_create(
        "{{cfg.label}}", ESP_RMAKER_DEVICE_LIGHTBULB, NULL);
    if (!s_{{prefix_lc}}_device) {
        ESP_LOGE(TAG, "Failed to create {{prefix_lc}} device");
        return ESP_FAIL;
    }
    esp_rmaker_device_add_cb(s_{{prefix_lc}}_device, rainmaker_write_cb, NULL);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device,
        esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "{{cfg.label}}"));
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.power_param}}, &init);
    s_{{prefix_lc}}_power_param =
        esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, init.b);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_power_param);
    app_driver_get_param({{cfg.brightness_param}}, &init);
    s_{{prefix_lc}}_brightness_param =
        esp_rmaker_brightness_param_create(ESP_RMAKER_DEF_BRIGHTNESS_NAME,
            ((int)init.u8 * 100) / 254);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_brightness_param);
    app_driver_get_param({{cfg.hue_param}}, &init);
    s_{{prefix_lc}}_hue_param =
        esp_rmaker_hue_param_create(ESP_RMAKER_DEF_HUE_NAME,
            ((int)init.u8 * 360) / 254);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_hue_param);
    app_driver_get_param({{cfg.saturation_param}}, &init);
    s_{{prefix_lc}}_saturation_param =
        esp_rmaker_saturation_param_create(ESP_RMAKER_DEF_SATURATION_NAME,
            ((int)init.u8 * 100) / 254);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_saturation_param);
    esp_rmaker_device_assign_primary_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_power_param);
    esp_rmaker_node_add_device(node, s_{{prefix_lc}}_device);
    ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker Lightbulb device '{{cfg.label}}'");
}
