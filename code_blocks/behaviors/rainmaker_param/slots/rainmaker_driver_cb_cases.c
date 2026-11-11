if (param_id == {{cfg.param}} && s_{{prefix_lc}}_rmp_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_rmp_param, {{prefix_lc}}_rmp_val(val));
}
