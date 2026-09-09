if (param_id == {{cfg.value_param}} && s_{{prefix_lc}}_humidity_param) {
    /* Driver carries hundredths of %RH (Matter encoding); RainMaker gets %RH. */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_humidity_param,
        esp_rmaker_float(val.i16 / 100.0f));
}
