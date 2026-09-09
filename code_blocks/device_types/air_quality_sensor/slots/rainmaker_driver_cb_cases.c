/* AirQuality enum: 0=Unknown, 1=Good, 2=Fair, 3=Moderate, 4=Poor, 5=VeryPoor, 6=ExtremelyPoor */
if (param_id == {{cfg.air_quality_param}} && s_{{prefix_lc}}_aqi_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_aqi_param, esp_rmaker_int(val.u8));
}
{{#if cfg.co2_ppm_param}}
if (param_id == {{cfg.co2_ppm_param}} && s_{{prefix_lc}}_co2_param) {
    /* Driver already carries natural units (ppm). */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_co2_param, esp_rmaker_int(val.u16));
}
{{/if}}
{{#if cfg.pm25_param}}
if (param_id == {{cfg.pm25_param}} && s_{{prefix_lc}}_pm25_param) {
    /* Driver already carries natural units (µg/m³). */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_pm25_param, esp_rmaker_int(val.u16));
}
{{/if}}
{{#if cfg.tvoc_param}}
if (param_id == {{cfg.tvoc_param}} && s_{{prefix_lc}}_tvoc_param) {
    /* Driver already carries natural units (ppb/µg·m⁻³). */
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_tvoc_param, esp_rmaker_int(val.u16));
}
{{/if}}
