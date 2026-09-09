if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_power_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_power_param, esp_rmaker_bool(val.b));
}
if (param_id == {{cfg.brightness_param}} && s_{{prefix_lc}}_brightness_param) {
    /* Driver brightness is Matter level units (0-254); RainMaker Brightness is 0-100. */
    int pct = ((int)val.u8 * 100) / 254;
    if (pct > 100) pct = 100;
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_brightness_param, esp_rmaker_int(pct));
}
if (param_id == {{cfg.color_temp_param}} && s_{{prefix_lc}}_cct_param) {
    /* Driver color temp is mireds (Matter); RainMaker CCT is Kelvin (2700-6500). */
    int kelvin = val.u16 > 0 ? 1000000 / (int)val.u16 : 4000;
    if (kelvin < 2700) kelvin = 2700;
    if (kelvin > 6500) kelvin = 6500;
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_cct_param, esp_rmaker_int(kelvin));
}
