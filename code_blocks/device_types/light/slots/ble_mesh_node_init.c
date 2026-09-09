{
    /* Seed the mesh's view from the hardware, so the first status a
     * controller reads is the truth rather than a zero. */
    app_driver_param_val_t {{prefix_lc}}_init = {};
    app_driver_get_param({{cfg.power_param}}, &{{prefix_lc}}_init);
    zc_ble_mesh_set_onoff_state({{prefix_lc}}_init.b);
}
