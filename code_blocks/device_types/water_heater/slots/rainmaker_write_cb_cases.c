if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_power_param) {
    app_driver_param_val_t pv = { .b = val.val.b };
    app_driver_set_param({{cfg.power_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_setpoint_param) {
    float deg = val.val.f < 30.0f ? 30.0f : (val.val.f > 80.0f ? 80.0f : val.val.f);
    app_driver_param_val_t pv = { .i16 = (int16_t)(deg * 100.0f) };
    app_driver_set_param({{cfg.setpoint_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
