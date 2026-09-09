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
    app_driver_get_param({{cfg.color_temp_param}}, &init);
    int {{prefix_lc}}_kelvin = init.u16 > 0 ? 1000000 / (int)init.u16 : 4000;
    if ({{prefix_lc}}_kelvin < 2700) {{prefix_lc}}_kelvin = 2700;
    if ({{prefix_lc}}_kelvin > 6500) {{prefix_lc}}_kelvin = 6500;
    s_{{prefix_lc}}_cct_param =
        esp_rmaker_cct_param_create(ESP_RMAKER_DEF_CCT_NAME, {{prefix_lc}}_kelvin);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_cct_param);
    esp_rmaker_device_assign_primary_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_power_param);
    esp_rmaker_node_add_device(node, s_{{prefix_lc}}_device);
    ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker Lightbulb device '{{cfg.label}}'");
}
