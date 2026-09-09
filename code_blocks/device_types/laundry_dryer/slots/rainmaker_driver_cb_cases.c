if (param_id == {{cfg.dryness_param}} && s_{{prefix_lc}}_dryness_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_dryness_param, esp_rmaker_int(val.u8));
}
if (param_id == {{cfg.state_param}} && s_{{prefix_lc}}_state_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_state_param, esp_rmaker_int(val.u8));
}
