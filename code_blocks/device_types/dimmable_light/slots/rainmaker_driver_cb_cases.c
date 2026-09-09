if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_power_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_power_param, esp_rmaker_bool(val.b));
}
if (param_id == {{cfg.brightness_param}} && s_{{prefix_lc}}_brightness_param) {
    /* Driver brightness is Matter level units (0-254); RainMaker Brightness is 0-100. */
    int pct = ((int)val.u8 * 100) / 254;
    if (pct > 100) pct = 100;
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_brightness_param, esp_rmaker_int(pct));
}
