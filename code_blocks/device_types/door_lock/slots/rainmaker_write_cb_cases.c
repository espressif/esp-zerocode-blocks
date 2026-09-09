if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_locked_param) {
    app_driver_param_val_t pv = { .b = val.val.b };
    app_driver_set_param({{cfg.locked_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
