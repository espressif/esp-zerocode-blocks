if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_power_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_power_param, esp_rmaker_bool(val.b));
}
if (param_id == {{cfg.cool_setpoint_param}} && s_{{prefix_lc}}_setpoint_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_setpoint_param,
        esp_rmaker_float(val.i16 / 100.0f));
}
if (param_id == {{cfg.fan_speed_param}} && s_{{prefix_lc}}_speed_param) {
    int pct = val.u8 > 100 ? 100 : val.u8;
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_speed_param, esp_rmaker_int(pct));
}
