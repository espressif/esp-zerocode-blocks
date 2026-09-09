if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_power_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_power_param, esp_rmaker_bool(val.b));
}
if (param_id == {{cfg.brightness_param}} && s_{{prefix_lc}}_brightness_param) {
    /* Driver brightness is Matter level units (0-254); RainMaker Brightness is 0-100. */
    int pct = ((int)val.u8 * 100) / 254;
    if (pct > 100) pct = 100;
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_brightness_param, esp_rmaker_int(pct));
}
if (param_id == {{cfg.hue_param}} && s_{{prefix_lc}}_hue_param) {
    /* Driver hue is Matter hue units (0-254); RainMaker Hue is 0-360. */
    int hue = ((int)val.u8 * 360) / 254;
    if (hue > 360) hue = 360;
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_hue_param, esp_rmaker_int(hue));
}
if (param_id == {{cfg.saturation_param}} && s_{{prefix_lc}}_saturation_param) {
    /* Driver saturation is Matter units (0-254); RainMaker Saturation is 0-100. */
    int sat = ((int)val.u8 * 100) / 254;
    if (sat > 100) sat = 100;
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_saturation_param, esp_rmaker_int(sat));
}
