{
    s_{{prefix_lc}}_rmp_device = esp_rmaker_device_create("{{cfg.label}}", ESP_RMAKER_DEVICE_OTHER, NULL);
    if (!s_{{prefix_lc}}_rmp_device) {
        ESP_LOGE(TAG, "Failed to create {{prefix_lc}} device");
        return ESP_FAIL;
    }
    esp_rmaker_device_add_cb(s_{{prefix_lc}}_rmp_device, rainmaker_write_cb, NULL);
    esp_rmaker_device_add_param(s_{{prefix_lc}}_rmp_device,
        esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "{{cfg.label}}"));
    app_driver_param_val_t {{prefix_lc}}_init = {};
    app_driver_get_param({{cfg.param}}, &{{prefix_lc}}_init);
    /* READ (and WRITE for a control); never PERSIST: the bus is the source of truth. */
#if defined({{prefix}}_RMP_KIND_value)
    uint8_t {{prefix_lc}}_props = PROP_FLAG_READ;
#else
    uint8_t {{prefix_lc}}_props = PROP_FLAG_READ | PROP_FLAG_WRITE;
#endif
    s_{{prefix_lc}}_rmp_param = esp_rmaker_param_create("{{cfg.label}}", "esp.param.zc-setting",
        {{prefix_lc}}_rmp_val({{prefix_lc}}_init), {{prefix_lc}}_props);
#if defined({{prefix}}_RMP_KIND_toggle)
    esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_rmp_param, ESP_RMAKER_UI_TOGGLE);
#elif defined({{prefix}}_RMP_KIND_slider) && defined({{prefix}}_RMP_SCALED)
    esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_rmp_param, ESP_RMAKER_UI_SLIDER);
    esp_rmaker_param_add_bounds(s_{{prefix_lc}}_rmp_param, esp_rmaker_float((float){{cfg.min}} / {{prefix}}_RMP_SCALE),
        esp_rmaker_float((float){{cfg.max}} / {{prefix}}_RMP_SCALE), esp_rmaker_float((float){{cfg.step}} / {{prefix}}_RMP_SCALE));
#elif defined({{prefix}}_RMP_KIND_slider)
    esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_rmp_param, ESP_RMAKER_UI_SLIDER);
    esp_rmaker_param_add_bounds(s_{{prefix_lc}}_rmp_param, esp_rmaker_int({{cfg.min}}), esp_rmaker_int({{cfg.max}}), esp_rmaker_int({{cfg.step}}));
#elif defined({{prefix}}_RMP_KIND_number) && defined({{prefix}}_RMP_SCALED)
    esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_rmp_param, ESP_RMAKER_UI_TEXT);
    esp_rmaker_param_add_bounds(s_{{prefix_lc}}_rmp_param, esp_rmaker_float((float){{cfg.min}} / {{prefix}}_RMP_SCALE),
        esp_rmaker_float((float){{cfg.max}} / {{prefix}}_RMP_SCALE), esp_rmaker_float(1.0f / {{prefix}}_RMP_SCALE));
#elif defined({{prefix}}_RMP_KIND_number)
    esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_rmp_param, ESP_RMAKER_UI_TEXT);
    esp_rmaker_param_add_bounds(s_{{prefix_lc}}_rmp_param, esp_rmaker_int({{cfg.min}}), esp_rmaker_int({{cfg.max}}), esp_rmaker_int(1));
#else
    esp_rmaker_param_add_ui_type(s_{{prefix_lc}}_rmp_param, ESP_RMAKER_UI_TEXT);
#endif
    esp_rmaker_device_add_param(s_{{prefix_lc}}_rmp_device, s_{{prefix_lc}}_rmp_param);
    esp_rmaker_device_assign_primary_param(s_{{prefix_lc}}_rmp_device, s_{{prefix_lc}}_rmp_param);
    esp_rmaker_node_add_device(node, s_{{prefix_lc}}_rmp_device);
    ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker setting '{{cfg.label}}' for {{cfg.param}}");
}
