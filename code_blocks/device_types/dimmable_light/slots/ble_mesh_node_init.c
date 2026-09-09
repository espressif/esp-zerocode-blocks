{
    app_driver_param_val_t {{prefix_lc}}_init = {};
    app_driver_get_param({{cfg.power_param}}, &{{prefix_lc}}_init);
    zc_ble_mesh_set_onoff_state({{prefix_lc}}_init.b);
    app_driver_get_param({{cfg.brightness_param}}, &{{prefix_lc}}_init);
    zc_ble_mesh_set_level_state(zc_ble_mesh_level_from_u8({{prefix_lc}}_init.u8));
}
