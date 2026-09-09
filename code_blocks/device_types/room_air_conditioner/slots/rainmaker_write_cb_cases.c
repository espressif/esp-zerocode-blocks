if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_power_param) {
    app_driver_param_val_t pv = { .b = val.val.b };
    app_driver_set_param({{cfg.power_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_setpoint_param) {
    float deg = val.val.f < 16.0f ? 16.0f : (val.val.f > 32.0f ? 32.0f : val.val.f);
    app_driver_param_val_t pv = { .i16 = (int16_t)(deg * 100.0f) };
    app_driver_set_param({{cfg.cool_setpoint_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_speed_param) {
    int pct = val.val.i < 0 ? 0 : (val.val.i > 100 ? 100 : val.val.i);
    app_driver_param_val_t pv = { .u8 = (uint8_t)pct };
    app_driver_set_param({{cfg.fan_speed_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
