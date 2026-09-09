if (event_base == NETWORK_PROV_EVENT && event_id == NETWORK_PROV_START) {
    app_driver_param_val_t v = { .u8 = 3 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
if (event_base == NETWORK_PROV_EVENT && event_id == NETWORK_PROV_END) {
    app_driver_param_val_t v = { .u8 = 1 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
if (event_base == RMAKER_COMMON_EVENT && event_id == RMAKER_MQTT_EVENT_CONNECTED) {
    app_driver_param_val_t v = { .u8 = 1 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
if (event_base == RMAKER_COMMON_EVENT && event_id == RMAKER_MQTT_EVENT_DISCONNECTED) {
    app_driver_param_val_t v = { .u8 = 4 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
