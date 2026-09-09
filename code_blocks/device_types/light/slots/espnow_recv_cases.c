if (hash == zc_now_hash("{{cfg.power_param}}") && msg.type == 0) {
    app_driver_param_val_t v = { .b = (msg.value != 0) };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    return;
}
