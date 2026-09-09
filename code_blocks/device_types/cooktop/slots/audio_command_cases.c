/* "turn off the cooktop" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .b = false };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
