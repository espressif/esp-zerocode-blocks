if (param_id == {{cfg.value_param}} && s_{{prefix_lc}}_moisture_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_moisture_param,
        esp_rmaker_int((int)val.u8));
}
