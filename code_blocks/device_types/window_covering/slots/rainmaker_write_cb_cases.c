if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_position_param) {
    int pct = val.val.i < 0 ? 0 : (val.val.i > 100 ? 100 : val.val.i);
    app_driver_param_val_t pv = { .u16 = (uint16_t)(pct * 100) };
    app_driver_set_param({{cfg.position_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
