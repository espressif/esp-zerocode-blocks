if (param_id == {{cfg.mode_param}} && s_{{prefix_lc}}_mode_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_mode_param, esp_rmaker_int(val.u8));
}
