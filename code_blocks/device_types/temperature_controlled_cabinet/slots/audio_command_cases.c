/* "make the cabinet colder" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.setpoint_param}}, &cur);
    int32_t n = (int32_t)cur.i16 + (-100);
    if (n < -2400) n = -2400;
    if (n > 1000) n = 1000;
    app_driver_param_val_t v = { .i16 = (int16_t)n };
    app_driver_set_param({{cfg.setpoint_param}}, v, s_handle);
    return;
}
/* "make the cabinet warmer" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.setpoint_param}}, &cur);
    int32_t n = (int32_t)cur.i16 + (+100);
    if (n < -2400) n = -2400;
    if (n > 1000) n = 1000;
    app_driver_param_val_t v = { .i16 = (int16_t)n };
    app_driver_set_param({{cfg.setpoint_param}}, v, s_handle);
    return;
}
