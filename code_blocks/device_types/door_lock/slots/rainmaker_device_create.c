{
    s_{{prefix_lc}}_device = esp_rmaker_device_create(
        "{{cfg.label}}", ESP_RMAKER_DEVICE_LOCK, NULL);
    if (!s_{{prefix_lc}}_device) {
        ESP_LOGE(TAG, "Failed to create {{prefix_lc}} device");
        return ESP_FAIL;
    }
    esp_rmaker_device_add_cb(s_{{prefix_lc}}_device, rainmaker_write_cb, NULL);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device,
        esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "{{cfg.label}}"));
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.locked_param}}, &init);
    /* No standard lock param creator exists — use the generic toggle param type. */
    s_{{prefix_lc}}_locked_param = esp_rmaker_param_create(
        "Locked", ESP_RMAKER_PARAM_TOGGLE,
        esp_rmaker_bool(init.b), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (s_{{prefix_lc}}_locked_param) {
        esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_locked_param, ESP_RMAKER_UI_TOGGLE);
    }
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_locked_param);
    esp_rmaker_device_assign_primary_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_locked_param);
    esp_rmaker_node_add_device(node, s_{{prefix_lc}}_device);
    ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker Lock device '{{cfg.label}}'");
}
