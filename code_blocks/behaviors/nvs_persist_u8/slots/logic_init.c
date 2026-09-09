{
    app_driver_register_solution("{{prefix_lc}}_persist", {{prefix_lc}}_persist_cb, NULL);
    nvs_handle_t h;
    if (nvs_open({{prefix}}_NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        uint8_t saved = 0;
        if (nvs_get_u8(h, {{prefix}}_NVS_KEY, &saved) == ESP_OK) {
            ESP_LOGI(TAG, "{{prefix_lc}}: restoring {{cfg.target_param}}=%u from NVS", saved);
            app_driver_param_val_t pv = { .u8 = saved };
            app_driver_set_param({{cfg.target_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
        }
        nvs_close(h);
    }
}
