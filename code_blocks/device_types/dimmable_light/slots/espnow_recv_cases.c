if (hash == zc_now_hash("{{cfg.power_param}}") && msg.type == 0) {
    app_driver_param_val_t v = { .b = (msg.value != 0) };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
if (hash == zc_now_hash("{{cfg.brightness_param}}") && msg.type == 1) {
    uint32_t b = msg.value;
    if (b < 1) b = 1;
    if (b > 254) b = 254;
    app_driver_param_val_t v = { .u8 = (uint8_t)b };
    app_driver_set_param({{cfg.brightness_param}}, v, s_handle);
    return;
}
