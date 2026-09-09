case {{cfg.power_param}}:
    s_{{prefix_lc}}_on = val.b;
    {{prefix_lc}}_apply();
    return ESP_OK;
case {{cfg.brightness_param}}:
    s_{{prefix_lc}}_brightness = val.u8;
    {{prefix_lc}}_apply();
    return ESP_OK;
case {{cfg.hue_param}}:
    s_{{prefix_lc}}_hue = val.u8;
    {{prefix_lc}}_apply();
    return ESP_OK;
case {{cfg.saturation_param}}:
    s_{{prefix_lc}}_sat = val.u8;
    {{prefix_lc}}_apply();
    return ESP_OK;
