case {{cfg.pattern_param}}:
    /* Single code path: every LED effect lands here and routes through the
       one led_indicator handle. Never raw-drive this pin elsewhere. */
    {{prefix_lc}}_status_led_set_pattern(val.u8);
    return ESP_OK;
