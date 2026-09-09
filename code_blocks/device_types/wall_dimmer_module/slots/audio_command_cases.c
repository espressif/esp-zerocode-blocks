/* "turn on the dimmer" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .b = true };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
/* "turn off the dimmer" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t v = { .b = false };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
/* "make it brighter" */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.brightness_param}}, &cur);
    int n = (int)cur.u8 + (+40);
    if (n < 1) n = 1;
    if (n > 254) n = 254;
    app_driver_param_val_t v = { .u8 = (uint8_t)n };
    app_driver_set_param({{cfg.brightness_param}}, v, s_handle);
    return;
}
/* "make it dimmer" */
case {{cfg.audio_cmd_base}} + 3: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.brightness_param}}, &cur);
    int n = (int)cur.u8 + (-40);
    if (n < 1) n = 1;
    if (n > 254) n = 254;
    app_driver_param_val_t v = { .u8 = (uint8_t)n };
    app_driver_set_param({{cfg.brightness_param}}, v, s_handle);
    return;
}
