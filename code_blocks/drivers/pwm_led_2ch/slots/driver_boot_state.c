{
    /* The light boots off at these levels: write them onto the params as
     * state, so a hub or app reads what the light will do from the start
     * instead of "never written" (a hub read level 0 while the first
     * switch-on came up at full). A value already written — a restore by
     * an earlier block — is left alone, and a later restore still applies. */
    if (!app_driver_param_seen({{cfg.power_param}})) {
        app_driver_param_val_t {{prefix_lc}}_v = {};
        {{prefix_lc}}_v.b = false;
        app_driver_set_param({{cfg.power_param}}, {{prefix_lc}}_v, APP_DRIVER_SOURCE_RESTORE);
    }
    if (!app_driver_param_seen({{cfg.brightness_param}})) {
        app_driver_param_val_t {{prefix_lc}}_v = {};
        {{prefix_lc}}_v.u8 = 254;
        app_driver_set_param({{cfg.brightness_param}}, {{prefix_lc}}_v, APP_DRIVER_SOURCE_RESTORE);
    }
    if (!app_driver_param_seen({{cfg.color_temp_param}})) {
        app_driver_param_val_t {{prefix_lc}}_v = {};
        {{prefix_lc}}_v.u16 = 250;
        app_driver_set_param({{cfg.color_temp_param}}, {{prefix_lc}}_v, APP_DRIVER_SOURCE_RESTORE);
    }
}
