case {{cfg.power_param}}:
    s_{{prefix_lc}}_on = val.b;
    {{prefix_lc}}_apply();
    return ESP_OK;
case {{cfg.brightness_param}}:
    s_{{prefix_lc}}_brightness = val.u8;
    {{prefix_lc}}_apply();
    return ESP_OK;
case {{cfg.color_temp_param}}:
    s_{{prefix_lc}}_mireds = val.u16;
    {{prefix_lc}}_apply();
    return ESP_OK;
