if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_setpoint_param) {
    float deg = val.val.f < 7.0f ? 7.0f : (val.val.f > 30.0f ? 30.0f : val.val.f);
    app_driver_param_val_t pv = { .i16 = (int16_t)(deg * 100.0f) };
    app_driver_set_param({{cfg.heat_setpoint_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_mode_param) {
    int mode = val.val.i < 0 ? 0 : (val.val.i > 4 ? 4 : val.val.i);
    app_driver_param_val_t pv = { .u8 = (uint8_t)mode };
    app_driver_set_param({{cfg.system_mode_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
