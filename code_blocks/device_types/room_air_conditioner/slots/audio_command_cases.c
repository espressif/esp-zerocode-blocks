/* "turn on the air conditioner" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .b = true };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
/* "turn off the air conditioner" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t v = { .b = false };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
/* "make it cooler" */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.cool_setpoint_param}}, &cur);
    int32_t n = (int32_t)cur.i16 + (-100);
    if (n < 1600) n = 1600;
    if (n > 3200) n = 3200;
    app_driver_param_val_t v = { .i16 = (int16_t)n };
    app_driver_set_param({{cfg.cool_setpoint_param}}, v, s_handle);
    return;
}
/* "make it warmer" */
case {{cfg.audio_cmd_base}} + 3: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.cool_setpoint_param}}, &cur);
    int32_t n = (int32_t)cur.i16 + (+100);
    if (n < 1600) n = 1600;
    if (n > 3200) n = 3200;
    app_driver_param_val_t v = { .i16 = (int16_t)n };
    app_driver_set_param({{cfg.cool_setpoint_param}}, v, s_handle);
    return;
}
