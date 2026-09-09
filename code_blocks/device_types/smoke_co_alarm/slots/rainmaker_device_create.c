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
    /* 0=Normal, 1=Warning, 2=Critical (same encoding as the driver params). */
    app_driver_get_param({{cfg.smoke_state_param}}, &init);
    s_{{prefix_lc}}_smoke_param = esp_rmaker_param_create(
        "Smoke State", NULL, esp_rmaker_int(init.u8), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_smoke_param);
    app_driver_get_param({{cfg.co_state_param}}, &init);
    s_{{prefix_lc}}_co_param = esp_rmaker_param_create(
        "CO State", NULL, esp_rmaker_int(init.u8), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_co_param);
    esp_rmaker_device_assign_primary_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_smoke_param);
    esp_rmaker_node_add_device(node, s_{{prefix_lc}}_device);
    ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker Smoke CO Alarm device '{{cfg.label}}'");
}
