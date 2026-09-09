if (param_id == {{cfg.value_param}} && s_{{prefix_lc}}_temperature_param) {
    /* Driver carries hundredths of °C (Matter encoding); RainMaker wants °C. */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_temperature_param,
        esp_rmaker_float(val.i16 / 100.0f));
}
