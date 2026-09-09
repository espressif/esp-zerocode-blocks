{
    /* Pre-onboarding default — assume unprovisioned at boot. Protocol
     * events (Matter kCommissioningComplete / RainMaker MQTT connected)
     * overwrite this once the device is set up. */
    app_driver_param_val_t v = { .u8 = 2 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
