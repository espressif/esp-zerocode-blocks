if (param_id == {{cfg.value_param}} && s_{{prefix_lc}}_value_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_value_param,
        esp_rmaker_float((float)val.i16 / {{cfg.scale}}));
}
