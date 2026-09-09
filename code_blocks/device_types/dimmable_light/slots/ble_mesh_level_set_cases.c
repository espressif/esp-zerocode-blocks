{
    app_driver_param_val_t pv = { .u8 = zc_ble_mesh_level_to_u8(level) };
    app_driver_set_param({{cfg.brightness_param}}, pv, s_handle);
}
