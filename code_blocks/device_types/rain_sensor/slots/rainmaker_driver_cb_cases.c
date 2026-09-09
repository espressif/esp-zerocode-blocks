if (param_id == {{cfg.rain_param}} && s_{{prefix_lc}}_rain_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_rain_param, esp_rmaker_bool(val.b));
}
