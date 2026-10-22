{
    /* Voice devices are "online" from boot — no onboarding to wait for. Only
     * when nothing has claimed the pattern yet: beside a transport,
     * commissioning_led's "waiting to be set up" is the truer signal. */
    if (!app_driver_param_seen({{cfg.pattern_param}})) {
        app_driver_param_val_t v = { .u8 = 1 };
        app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    }
}
