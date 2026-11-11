{
    /* GPIO -> (unit, channel), adopt-or-create the unit, then a calibration
       scheme for millivolts. Fail CLOSED: no ADC on the pin leaves the reading
       disabled, and this line is the report. */
    adc_unit_t {{prefix_lc}}_unit_id = ADC_UNIT_1;
    adc_channel_t {{prefix_lc}}_chan = (adc_channel_t)0;
    adc_oneshot_unit_handle_t {{prefix_lc}}_unit = NULL;
    esp_err_t {{prefix_lc}}_err = adc_oneshot_io_to_channel({{prefix}}_AIN_GPIO, &{{prefix_lc}}_unit_id, &{{prefix_lc}}_chan);
    if ({{prefix_lc}}_err == ESP_OK) {
        {{prefix_lc}}_err = zc_adc_unit_acquire({{prefix_lc}}_unit_id, &{{prefix_lc}}_unit);
    }
    if ({{prefix_lc}}_err == ESP_OK) {
        adc_oneshot_chan_cfg_t {{prefix_lc}}_chan_cfg = {};
        {{prefix_lc}}_chan_cfg.atten = (adc_atten_t){{prefix}}_AIN_ATTEN;
        {{prefix_lc}}_chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
        {{prefix_lc}}_err = adc_oneshot_config_channel({{prefix_lc}}_unit, {{prefix_lc}}_chan, &{{prefix_lc}}_chan_cfg);
    }
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "analog input {{prefix_lc}}: GPIO%d has no usable ADC channel (%s) - reading disabled",
                 {{prefix}}_AIN_GPIO, esp_err_to_name({{prefix_lc}}_err));
    } else {
        s_{{prefix_lc}}_adc = {{prefix_lc}}_unit;
        s_{{prefix_lc}}_chan = {{prefix_lc}}_chan;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_curve_fitting_config_t {{prefix_lc}}_cc = {};
        {{prefix_lc}}_cc.unit_id = {{prefix_lc}}_unit_id;
        {{prefix_lc}}_cc.chan = {{prefix_lc}}_chan;
        {{prefix_lc}}_cc.atten = (adc_atten_t){{prefix}}_AIN_ATTEN;
        {{prefix_lc}}_cc.bitwidth = ADC_BITWIDTH_DEFAULT;
        if (adc_cali_create_scheme_curve_fitting(&{{prefix_lc}}_cc, &s_{{prefix_lc}}_cali) != ESP_OK) s_{{prefix_lc}}_cali = NULL;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_line_fitting_config_t {{prefix_lc}}_lc = {};
        {{prefix_lc}}_lc.unit_id = {{prefix_lc}}_unit_id;
        {{prefix_lc}}_lc.atten = (adc_atten_t){{prefix}}_AIN_ATTEN;
        {{prefix_lc}}_lc.bitwidth = ADC_BITWIDTH_DEFAULT;
        if (adc_cali_create_scheme_line_fitting(&{{prefix_lc}}_lc, &s_{{prefix_lc}}_cali) != ESP_OK) s_{{prefix_lc}}_cali = NULL;
#endif
        ESP_LOGI(TAG, "analog input {{prefix_lc}}: GPIO%d = ADC%d ch%d, %s, every %d ms",
                 {{prefix}}_AIN_GPIO, (int){{prefix_lc}}_unit_id + 1, (int){{prefix_lc}}_chan,
                 s_{{prefix_lc}}_cali ? "calibrated mV" : "nominal mV (no calibration scheme)", {{prefix}}_AIN_POLL_MS);
        if (app_driver_poll_every(&{{prefix_lc}}_ain_poll, NULL, (uint32_t){{prefix}}_AIN_POLL_MS) != ESP_OK) {
            ESP_LOGE(TAG, "analog input {{prefix_lc}}: could not start polling - reading disabled");
        }
    }
}
