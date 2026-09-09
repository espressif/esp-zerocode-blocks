if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_power_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_power_param, esp_rmaker_bool(val.b));
}
if (param_id == {{cfg.setpoint_param}} && s_{{prefix_lc}}_setpoint_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_setpoint_param,
        esp_rmaker_float(val.i16 / 100.0f));
}
