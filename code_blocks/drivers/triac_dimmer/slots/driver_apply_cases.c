case {{cfg.power_param}}:
    s_{{prefix_lc}}_on = val.b;
    return ESP_OK;
case {{cfg.brightness_param}}:
    s_{{prefix_lc}}_brightness = val.u8;
    return ESP_OK;
