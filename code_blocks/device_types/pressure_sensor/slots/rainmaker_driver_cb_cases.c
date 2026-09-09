if (param_id == {{cfg.value_param}} && s_{{prefix_lc}}_pressure_param) {
    /* Driver carries 10ths of kPa (Matter encoding); RainMaker gets kPa. */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_pressure_param,
        esp_rmaker_float(val.i16 / 10.0f));
}
