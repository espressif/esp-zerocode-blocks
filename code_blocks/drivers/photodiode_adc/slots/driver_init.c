{
    /* Adopt-or-create the unit (see zc_adc_unit_acquire), and fail CLOSED:
       one absent light reading must not boot-loop the product. The poll then
       finds no handle and stays quiet; this line is the report. */
    adc_oneshot_unit_handle_t {{prefix_lc}}_unit = NULL;
    esp_err_t {{prefix_lc}}_err = zc_adc_unit_acquire((adc_unit_t){{prefix}}_ADC_UNIT, &{{prefix_lc}}_unit);
    if ({{prefix_lc}}_err == ESP_OK) {
        adc_oneshot_chan_cfg_t {{prefix_lc}}_chan_cfg = {
            .atten = (adc_atten_t){{prefix}}_ADC_ATTEN,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        {{prefix_lc}}_err = adc_oneshot_config_channel({{prefix_lc}}_unit, (adc_channel_t){{prefix}}_ADC_CHANNEL, &{{prefix_lc}}_chan_cfg);
    }
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "ADC photodiode {{prefix_lc}}: unit=%d ch=%d unavailable (%s) — light reading disabled",
                 {{prefix}}_ADC_UNIT, {{prefix}}_ADC_CHANNEL, esp_err_to_name({{prefix_lc}}_err));
    } else {
        s_{{prefix_lc}}_adc = {{prefix_lc}}_unit;
        ESP_LOGI(TAG, "ADC photodiode {{prefix_lc}}: unit=%d ch=%d", {{prefix}}_ADC_UNIT, {{prefix}}_ADC_CHANNEL);
    }
}
