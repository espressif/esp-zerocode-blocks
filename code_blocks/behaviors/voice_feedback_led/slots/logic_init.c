{
    /* Voice devices are "online" from boot — no onboarding to wait for. */
    app_driver_param_val_t v = { .u8 = 1 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
