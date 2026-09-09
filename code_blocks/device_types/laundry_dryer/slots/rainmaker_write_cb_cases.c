if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_dryness_param) {
    int level = val.val.i < 0 ? 0 : (val.val.i > 3 ? 3 : val.val.i);
    app_driver_param_val_t pv = { .u8 = (uint8_t)level };
    app_driver_set_param({{cfg.dryness_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
