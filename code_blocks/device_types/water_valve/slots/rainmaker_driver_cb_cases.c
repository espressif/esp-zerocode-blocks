if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_power_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_power_param, esp_rmaker_bool(val.b));
}
