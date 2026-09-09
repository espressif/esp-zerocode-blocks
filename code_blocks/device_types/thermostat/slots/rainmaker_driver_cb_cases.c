if (param_id == {{cfg.local_temp_param}} && s_{{prefix_lc}}_temp_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_temp_param,
        esp_rmaker_float(val.i16 / 100.0f));
}
if (param_id == {{cfg.heat_setpoint_param}} && s_{{prefix_lc}}_setpoint_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_setpoint_param,
        esp_rmaker_float(val.i16 / 100.0f));
}
if (param_id == {{cfg.system_mode_param}} && s_{{prefix_lc}}_mode_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_mode_param, esp_rmaker_int(val.u8));
}
