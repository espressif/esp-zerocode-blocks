/* A device already on a network rejoins through DEVICE_REBOOT (or FIRST_START)
 * and no steering runs (the framework only steers a factory-new device), so
 * without this a paired device's LED stayed on logic_init's "waiting to be set
 * up" blink after every reboot. */
if ((sig_type == EZB_BDB_SIGNAL_DEVICE_FIRST_START || sig_type == EZB_BDB_SIGNAL_DEVICE_REBOOT) &&
    err_status == EZB_BDB_STATUS_SUCCESS && !ezb_bdb_is_factory_new()) {
    app_driver_param_val_t v = { .u8 = 1 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
if (sig_type == EZB_BDB_SIGNAL_STEERING) {
    app_driver_param_val_t v = {
        .u8 = (uint8_t)(err_status == EZB_BDB_STATUS_SUCCESS ? 1 : 3)
    };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
if (sig_type == EZB_ZDO_SIGNAL_LEAVE) {
    app_driver_param_val_t v = { .u8 = 2 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
