{
    /* Pre-onboarding default — assume unprovisioned at boot. Protocol
     * events (Matter kCommissioningComplete / RainMaker MQTT connected)
     * overwrite this once the device is set up. Only when nothing has set
     * the pattern yet: a provisioned BLE Mesh node replays PROV_COMPLETE
     * from esp_ble_mesh_init(), and a joined Zigbee device signals
     * DEVICE_REBOOT, both from the main_init slot on higher-priority tasks that
     * can run before this — overwriting them left a set-up device on the
     * "waiting to be set up" blink. */
    if (!app_driver_param_seen({{cfg.pattern_param}})) {
        app_driver_param_val_t v = { .u8 = 2 };
        app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    }
}
