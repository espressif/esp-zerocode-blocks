static void {{prefix_lc}}_meter_poll_cb(void *arg)
{
    /* STUB: publishes a simulated reading (correct for dev firmware).
     * Do NOT add adc_oneshot/adc_cali code here — use drivers/hlw8012 or
     * drivers/ina219 for real metering. */
    app_driver_param_val_t v = { .u32 = {{prefix}}_METER_INITIAL };
    app_driver_set_param({{cfg.power_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
