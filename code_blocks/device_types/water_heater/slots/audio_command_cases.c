/* "turn on the water heater" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .b = true };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
/* "turn off the water heater" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t v = { .b = false };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
/* "make the water hotter" */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.setpoint_param}}, &cur);
    int32_t n = (int32_t)cur.i16 + (+200);
    if (n < 3000) n = 3000;
    if (n > 7000) n = 7000;
    app_driver_param_val_t v = { .i16 = (int16_t)n };
    app_driver_set_param({{cfg.setpoint_param}}, v, s_handle);
    return;
}
/* "make the water cooler" */
case {{cfg.audio_cmd_base}} + 3: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.setpoint_param}}, &cur);
    int32_t n = (int32_t)cur.i16 + (-200);
    if (n < 3000) n = 3000;
    if (n > 7000) n = 7000;
    app_driver_param_val_t v = { .i16 = (int16_t)n };
    app_driver_set_param({{cfg.setpoint_param}}, v, s_handle);
    return;
}
