if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_setpoint_param) {
    float temp = val.val.f < -40.0f ? -40.0f : (val.val.f > 300.0f ? 300.0f : val.val.f);
    app_driver_param_val_t pv = { .i16 = (int16_t)(temp * 100.0f) };
    app_driver_set_param({{cfg.setpoint_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
