if (param_id == {{cfg.enabled_param}} && s_{{prefix_lc}}_enabled_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_enabled_param, esp_rmaker_bool(val.b));
}
