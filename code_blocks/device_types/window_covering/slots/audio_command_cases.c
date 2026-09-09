/* "open the blinds" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .u16 = 10000 };
    app_driver_set_param({{cfg.position_param}}, v, s_handle);
    return;
}
/* "close the blinds" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t v = { .u16 = 0 };
    app_driver_set_param({{cfg.position_param}}, v, s_handle);
    return;
}
/* "raise the blinds" */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.position_param}}, &cur);
    int n = (int)cur.u16 + (+2000);
    if (n < 0) n = 0;
    if (n > 10000) n = 10000;
    app_driver_param_val_t v = { .u16 = (uint16_t)n };
    app_driver_set_param({{cfg.position_param}}, v, s_handle);
    return;
}
/* "lower the blinds" */
case {{cfg.audio_cmd_base}} + 3: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.position_param}}, &cur);
    int n = (int)cur.u16 + (-2000);
    if (n < 0) n = 0;
    if (n > 10000) n = 10000;
    app_driver_param_val_t v = { .u16 = (uint16_t)n };
    app_driver_set_param({{cfg.position_param}}, v, s_handle);
    return;
}
