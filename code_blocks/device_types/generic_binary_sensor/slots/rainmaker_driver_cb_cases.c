if (param_id == {{cfg.value_param}} && s_{{prefix_lc}}_state_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_state_param, esp_rmaker_bool(val.b));
}
