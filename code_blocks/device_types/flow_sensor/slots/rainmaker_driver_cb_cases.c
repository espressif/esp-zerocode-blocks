if (param_id == {{cfg.value_param}} && s_{{prefix_lc}}_flow_param) {
    /* Driver carries 10ths of m³/h (Matter encoding); RainMaker gets m³/h. */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_flow_param,
        esp_rmaker_float(val.u16 / 10.0f));
}
