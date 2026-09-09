case {{cfg.audio_cmd_base}} + 0: {
    app_driver_param_val_t v = { .b = true };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
case {{cfg.audio_cmd_base}} + 1: {
    app_driver_param_val_t v = { .b = false };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
/* Relative steps read current level from the bus rather than tracking their
 * own copy — the console, a button or another framework may have moved it. */
case {{cfg.audio_cmd_base}} + 2: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.brightness_param}}, &cur);
    int lvl = (int)cur.u8 + 40;
    if (lvl > 254) lvl = 254;
    app_driver_param_val_t v = { .u8 = (uint8_t)lvl };
    app_driver_set_param({{cfg.brightness_param}}, v, s_handle);
    return;
}
case {{cfg.audio_cmd_base}} + 3: {
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.brightness_param}}, &cur);
    int lvl = (int)cur.u8 - 40;
    /* 1 is the floor, not 0: CurrentLevel reserves 0 and 255. */
    if (lvl < 1) lvl = 1;
    app_driver_param_val_t v = { .u8 = (uint8_t)lvl };
    app_driver_set_param({{cfg.brightness_param}}, v, s_handle);
    return;
}
case {{cfg.audio_cmd_base}} + 4: {
    app_driver_param_val_t v = { .u8 = 254 };
    app_driver_set_param({{cfg.brightness_param}}, v, s_handle);
    return;
}
case {{cfg.audio_cmd_base}} + 5: {
    app_driver_param_val_t v = { .u8 = 1 };
    app_driver_set_param({{cfg.brightness_param}}, v, s_handle);
    return;
}
