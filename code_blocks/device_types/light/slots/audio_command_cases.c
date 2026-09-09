case {{cfg.audio_cmd_base}} + 0:
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t v = { .b = true };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
case {{cfg.audio_cmd_base}} + 1:
case {{cfg.audio_cmd_base}} + 3: {
    app_driver_param_val_t v = { .b = false };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
