if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_power_param) {
    /* Driver power is signed watts (positive = charging). */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_power_param,
        esp_rmaker_float((float)val.i16));
}
if (param_id == {{cfg.energy_param}} && s_{{prefix_lc}}_energy_param) {
    /* Driver energy is Wh; RainMaker reports kWh. */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_energy_param,
        esp_rmaker_float(val.u32 / 1000.0f));
}
