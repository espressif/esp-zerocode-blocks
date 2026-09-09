if (param_id == {{cfg.active_power_param}} && s_{{prefix_lc}}_power_w_param) {
    /* Driver carries mW (u32); RainMaker gets W. */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_power_w_param,
        esp_rmaker_float(val.u32 / 1000.0f));
}
{{#if cfg.voltage_param}}
if (param_id == {{cfg.voltage_param}} && s_{{prefix_lc}}_voltage_param) {
    /* Driver carries mV (u32); RainMaker gets V. */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_voltage_param,
        esp_rmaker_float(val.u32 / 1000.0f));
}
{{/if}}
{{#if cfg.current_param}}
if (param_id == {{cfg.current_param}} && s_{{prefix_lc}}_current_param) {
    /* Driver carries mA (u32); RainMaker gets A. */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_current_param,
        esp_rmaker_float(val.u32 / 1000.0f));
}
{{/if}}
