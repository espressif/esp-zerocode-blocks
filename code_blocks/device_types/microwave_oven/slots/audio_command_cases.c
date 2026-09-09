/* "set the microwave to one minute" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .u32 = 60 };
    app_driver_set_param({{cfg.cook_time_param}}, v, s_handle);
    return;
}
/* "set the microwave to three minutes" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t v = { .u32 = 180 };
    app_driver_set_param({{cfg.cook_time_param}}, v, s_handle);
    return;
}
/* "set microwave power to full" */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t v = { .u8 = 100 };
    app_driver_set_param({{cfg.power_level_param}}, v, s_handle);
    return;
}
/* "set microwave power to half" */
case {{cfg.audio_cmd_base}} + 3: {
    app_driver_param_val_t v = { .u8 = 50 };
    app_driver_set_param({{cfg.power_level_param}}, v, s_handle);
    return;
}
