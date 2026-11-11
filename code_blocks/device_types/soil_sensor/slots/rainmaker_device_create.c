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
    app_driver_get_param({{cfg.value_param}}, &init);
    /* No standard soil-moisture param type — custom read-only int in %. */
    s_{{prefix_lc}}_moisture_param = esp_rmaker_param_create(
        "Moisture", NULL, esp_rmaker_int((int)init.u8), PROP_FLAG_READ);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_moisture_param);
    esp_rmaker_device_assign_primary_param(s_{{prefix_lc}}_device, s_{{prefix_lc}}_moisture_param);
    esp_rmaker_node_add_device(node, s_{{prefix_lc}}_device);
    ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker Soil Sensor device '{{cfg.label}}'");
}
