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
