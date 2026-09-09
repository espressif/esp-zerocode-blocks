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
if (device == s_{{prefix_lc}}_device && param == s_{{prefix_lc}}_cct_param) {
    /* RainMaker CCT is Kelvin; driver expects mireds (kelvin = 1000000 / mireds). */
    int kelvin = val.val.i < 2700 ? 2700 : (val.val.i > 6500 ? 6500 : val.val.i);
    app_driver_param_val_t pv = { .u16 = (uint16_t)(1000000 / kelvin) };
    app_driver_set_param({{cfg.color_temp_param}}, pv, s_handle);
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
