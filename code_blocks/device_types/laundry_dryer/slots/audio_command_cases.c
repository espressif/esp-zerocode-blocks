/* "set the dryer to low" */
case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .u8 = 0 };
    app_driver_set_param({{cfg.dryness_param}}, v, s_handle);
    return;
}
/* "set the dryer to normal" */
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t v = { .u8 = 1 };
    app_driver_set_param({{cfg.dryness_param}}, v, s_handle);
    return;
}
/* "set the dryer to extra" */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t v = { .u8 = 2 };
    app_driver_set_param({{cfg.dryness_param}}, v, s_handle);
    return;
}
