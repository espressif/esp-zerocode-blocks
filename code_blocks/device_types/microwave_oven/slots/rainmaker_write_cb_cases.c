if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_cook_time_param) {
    int secs = val.val.i < 0 ? 0 : (val.val.i > 3600 ? 3600 : val.val.i);
    app_driver_param_val_t pv = { .u32 = (uint32_t)secs };
    app_driver_set_param({{cfg.cook_time_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_power_level_param) {
    int level = val.val.i < 10 ? 10 : (val.val.i > 100 ? 100 : val.val.i);
    app_driver_param_val_t pv = { .u8 = (uint8_t)level };
    app_driver_set_param({{cfg.power_level_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
