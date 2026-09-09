{
    adc_oneshot_unit_init_cfg_t {{prefix_lc}}_unit_cfg = { .unit_id = (adc_unit_t){{prefix}}_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&{{prefix_lc}}_unit_cfg, &s_{{prefix_lc}}_adc));
    ESP_LOGI(TAG, "ADC unit {{prefix_lc}} initialized (unit=%d)", {{prefix}}_ADC_UNIT);
}
