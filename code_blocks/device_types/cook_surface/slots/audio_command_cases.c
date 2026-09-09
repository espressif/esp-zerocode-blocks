/* "turn off the burner" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .i16 = 0 };
    app_driver_set_param({{cfg.setpoint_param}}, v, s_handle);
    return;
}
/* "make the burner hotter" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.setpoint_param}}, &cur);
    int32_t n = (int32_t)cur.i16 + (+1000);
    if (n < 0) n = 0;
    if (n > 26000) n = 26000;
    app_driver_param_val_t v = { .i16 = (int16_t)n };
    app_driver_set_param({{cfg.setpoint_param}}, v, s_handle);
    return;
}
/* "make the burner cooler" */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.setpoint_param}}, &cur);
    int32_t n = (int32_t)cur.i16 + (-1000);
    if (n < 0) n = 0;
    if (n > 26000) n = 26000;
    app_driver_param_val_t v = { .i16 = (int16_t)n };
    app_driver_set_param({{cfg.setpoint_param}}, v, s_handle);
    return;
}
