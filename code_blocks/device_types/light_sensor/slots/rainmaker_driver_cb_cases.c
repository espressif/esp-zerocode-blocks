if (param_id == {{cfg.value_param}} && s_{{prefix_lc}}_lux_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_lux_param,
        esp_rmaker_float({{prefix_lc}}_encoded_to_lux(val.u16)));
}
