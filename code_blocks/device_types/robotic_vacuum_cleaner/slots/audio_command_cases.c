/* "start cleaning" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .u8 = 1 };
    app_driver_set_param({{cfg.run_mode_param}}, v, s_handle);
    return;
}
/* "stop cleaning" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t v = { .u8 = 0 };
    app_driver_set_param({{cfg.run_mode_param}}, v, s_handle);
    return;
}
/* "start mapping" */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t v = { .u8 = 2 };
    app_driver_set_param({{cfg.run_mode_param}}, v, s_handle);
    return;
}
