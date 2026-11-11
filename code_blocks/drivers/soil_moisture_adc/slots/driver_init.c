{
    /* GPIO → (unit, channel), then adopt-or-create the unit (see
       zc_adc_unit_acquire). Fail CLOSED: a probe on a pin with no ADC must not
       boot-loop the product — the poll finds no handle and stays quiet, and
       this line is the report. */
    adc_unit_t {{prefix_lc}}_unit_id = ADC_UNIT_1;
    adc_channel_t {{prefix_lc}}_chan = (adc_channel_t)0;
    adc_oneshot_unit_handle_t {{prefix_lc}}_unit = NULL;
    esp_err_t {{prefix_lc}}_err = adc_oneshot_io_to_channel({{prefix}}_SOIL_GPIO, &{{prefix_lc}}_unit_id, &{{prefix_lc}}_chan);
    if ({{prefix_lc}}_err == ESP_OK) {
        {{prefix_lc}}_err = zc_adc_unit_acquire({{prefix_lc}}_unit_id, &{{prefix_lc}}_unit);
    }
    if ({{prefix_lc}}_err == ESP_OK) {
        adc_oneshot_chan_cfg_t {{prefix_lc}}_chan_cfg = {
            .atten = (adc_atten_t){{prefix}}_SOIL_ATTEN,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        {{prefix_lc}}_err = adc_oneshot_config_channel({{prefix_lc}}_unit, {{prefix_lc}}_chan, &{{prefix_lc}}_chan_cfg);
    }
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "soil probe {{prefix_lc}}: GPIO%d has no usable ADC channel (%s) — moisture reading disabled",
                 {{prefix}}_SOIL_GPIO, esp_err_to_name({{prefix_lc}}_err));
    } else {
        s_{{prefix_lc}}_adc = {{prefix_lc}}_unit;
        s_{{prefix_lc}}_adc_channel = {{prefix_lc}}_chan;
        ESP_LOGI(TAG, "soil probe {{prefix_lc}}: GPIO%d = ADC%d ch%d (built-in dry=%d wet=%d; soil-cal changes them)",
                 {{prefix}}_SOIL_GPIO, (int){{prefix_lc}}_unit_id + 1, (int){{prefix_lc}}_chan,
                 {{prefix}}_SOIL_DRY_RAW, {{prefix}}_SOIL_WET_RAW);
    }
}
