/* "turn on the purifier" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .u8 = 100 };
    app_driver_set_param({{cfg.speed_param}}, v, s_handle);
    return;
}
/* "turn off the purifier" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t v = { .u8 = 0 };
    app_driver_set_param({{cfg.speed_param}}, v, s_handle);
    return;
}
/* "purifier faster" */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.speed_param}}, &cur);
    int n = (int)cur.u8 + (+25);
    if (n < 0) n = 0;
    if (n > 100) n = 100;
    app_driver_param_val_t v = { .u8 = (uint8_t)n };
    app_driver_set_param({{cfg.speed_param}}, v, s_handle);
    return;
}
/* "purifier slower" */
case {{cfg.audio_cmd_base}} + 3: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.speed_param}}, &cur);
    int n = (int)cur.u8 + (-25);
    if (n < 0) n = 0;
    if (n > 100) n = 100;
    app_driver_param_val_t v = { .u8 = (uint8_t)n };
    app_driver_set_param({{cfg.speed_param}}, v, s_handle);
    return;
}
