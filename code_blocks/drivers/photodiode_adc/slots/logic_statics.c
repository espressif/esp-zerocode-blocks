extern adc_oneshot_unit_handle_t s_{{prefix_lc}}_adc;

static void {{prefix_lc}}_adc_poll_cb(void *arg)
{
    int raw = 0;
    if (adc_oneshot_read(s_{{prefix_lc}}_adc, (adc_channel_t){{prefix}}_ADC_CHANNEL, &raw) != ESP_OK) return;
    if (raw < 0) raw = 0;
    if (raw > {{prefix}}_ADC_RAW_MAX) raw = {{prefix}}_ADC_RAW_MAX;
    uint32_t scaled = ((uint32_t)raw * (uint32_t){{prefix}}_ADC_SCALE) / (uint32_t){{prefix}}_ADC_RAW_MAX;
    if (scaled > 0xFFFF) scaled = 0xFFFF;
    app_driver_param_val_t v = { .u16 = (uint16_t)scaled };
    app_driver_set_param({{cfg.value_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
