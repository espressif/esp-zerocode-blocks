{
    adc_oneshot_unit_init_cfg_t {{prefix_lc}}_unit_cfg = { .unit_id = (adc_unit_t){{prefix}}_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&{{prefix_lc}}_unit_cfg, &s_{{prefix_lc}}_adc));
    adc_oneshot_chan_cfg_t {{prefix_lc}}_chan_cfg = {
        .atten = (adc_atten_t){{prefix}}_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_{{prefix_lc}}_adc, (adc_channel_t){{prefix}}_ADC_CHANNEL, &{{prefix_lc}}_chan_cfg));
    ESP_LOGI(TAG, "ADC photodiode {{prefix_lc}}: unit=%d ch=%d", {{prefix}}_ADC_UNIT, {{prefix}}_ADC_CHANNEL);
}
