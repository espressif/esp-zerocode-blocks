case {{cfg.power_param}}:
    s_{{prefix_lc}}_led_on = val.b;
    {{prefix_lc}}_led_apply_duty();
    return ESP_OK;
case {{cfg.brightness_param}}:
    s_{{prefix_lc}}_led_brightness = val.u8;
    {{prefix_lc}}_led_apply_duty();
    return ESP_OK;
