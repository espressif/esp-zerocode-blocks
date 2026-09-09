{
    /* listening: 1 = command window open (wake word heard), 0 = closed. */
    app_driver_param_val_t v = { .u8 = (uint8_t)(listening ? 3 : 1) };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
