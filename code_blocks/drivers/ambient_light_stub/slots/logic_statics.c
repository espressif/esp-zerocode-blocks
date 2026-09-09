static void {{prefix_lc}}_poll_cb(void *arg)
{
    /* STUB: replace with ADC oneshot or I2C illuminance read for your sensor. */
    app_driver_param_val_t v = { .u16 = {{prefix}}_INITIAL_VALUE };
    app_driver_set_param({{cfg.value_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
