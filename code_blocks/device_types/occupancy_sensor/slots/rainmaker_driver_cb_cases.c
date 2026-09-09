if (param_id == {{cfg.occupied_param}} && s_{{prefix_lc}}_occupied_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_occupied_param, esp_rmaker_bool(val.b));
}
