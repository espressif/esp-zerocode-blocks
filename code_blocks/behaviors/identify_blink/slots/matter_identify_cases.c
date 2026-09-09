if (type == esp_matter::identification::callback_type_t::START) {
    app_driver_param_val_t v = { .u8 = {{prefix}}_IDENTIFY_PATTERN };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
} else if (type == esp_matter::identification::callback_type_t::STOP) {
    app_driver_param_val_t v = { .u8 = {{prefix}}_IDLE_PATTERN };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
