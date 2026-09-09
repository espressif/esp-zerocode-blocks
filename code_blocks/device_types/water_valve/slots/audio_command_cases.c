/* "open the valve" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .b = true };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
/* "close the valve" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t v = { .b = false };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
