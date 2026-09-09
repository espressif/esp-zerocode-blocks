{
    /* connected: 1 = broker session up → solid; 0 = lost → breathe. */
    app_driver_param_val_t v = { .u8 = (uint8_t)(connected ? 1 : 4) };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
