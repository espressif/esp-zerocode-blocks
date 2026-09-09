if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_power_param) {
    app_driver_param_val_t pv = { .b = val.val.b };
    app_driver_set_param({{cfg.power_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_brightness_param) {
    int pct = val.val.i < 0 ? 0 : (val.val.i > 100 ? 100 : val.val.i);
    app_driver_param_val_t pv = { .u8 = (uint8_t)((pct * 254) / 100) };
    app_driver_set_param({{cfg.brightness_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_hue_param) {
    int hue = val.val.i < 0 ? 0 : (val.val.i > 360 ? 360 : val.val.i);
    app_driver_param_val_t pv = { .u8 = (uint8_t)((hue * 254) / 360) };
    app_driver_set_param({{cfg.hue_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_saturation_param) {
    int sat = val.val.i < 0 ? 0 : (val.val.i > 100 ? 100 : val.val.i);
    app_driver_param_val_t pv = { .u8 = (uint8_t)((sat * 254) / 100) };
    app_driver_set_param({{cfg.saturation_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
