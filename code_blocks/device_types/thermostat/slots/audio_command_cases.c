/* "make it warmer" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.heat_setpoint_param}}, &cur);
    int32_t n = (int32_t)cur.i16 + (+100);
    if (n < 500) n = 500;
    if (n > 3500) n = 3500;
    app_driver_param_val_t v = { .i16 = (int16_t)n };
    app_driver_set_param({{cfg.heat_setpoint_param}}, v, s_handle);
    return;
}
/* "make it cooler" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.heat_setpoint_param}}, &cur);
    int32_t n = (int32_t)cur.i16 + (-100);
    if (n < 500) n = 500;
    if (n > 3500) n = 3500;
    app_driver_param_val_t v = { .i16 = (int16_t)n };
    app_driver_set_param({{cfg.heat_setpoint_param}}, v, s_handle);
    return;
}
/* "turn on the heating" */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t v = { .u8 = 4 };
    app_driver_set_param({{cfg.system_mode_param}}, v, s_handle);
    return;
}
/* "turn off the heating" */
case {{cfg.audio_cmd_base}} + 3: {
    app_driver_param_val_t v = { .u8 = 0 };
    app_driver_set_param({{cfg.system_mode_param}}, v, s_handle);
    return;
}
