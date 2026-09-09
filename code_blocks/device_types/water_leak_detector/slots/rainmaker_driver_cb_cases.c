if (param_id == {{cfg.leak_param}} && s_{{prefix_lc}}_leak_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_leak_param, esp_rmaker_bool(val.b));
}
