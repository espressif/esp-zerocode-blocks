if (param_id == {{cfg.cook_time_param}} && s_{{prefix_lc}}_cook_time_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_cook_time_param, esp_rmaker_int((int)val.u32));
}
if (param_id == {{cfg.power_level_param}} && s_{{prefix_lc}}_power_level_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_power_level_param, esp_rmaker_int(val.u8));
}
if (param_id == {{cfg.state_param}} && s_{{prefix_lc}}_state_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_state_param, esp_rmaker_int(val.u8));
}
