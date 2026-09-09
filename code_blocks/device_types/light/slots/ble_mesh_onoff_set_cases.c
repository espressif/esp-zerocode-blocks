{
    /* A controller wrote our Generic OnOff state → drive the hardware. */
    app_driver_param_val_t pv = { .b = (onoff != 0) };
    app_driver_set_param({{cfg.power_param}}, pv, s_handle);
}
