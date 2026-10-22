{
    /* Shared with every ADC driver in the product (zc_adc_unit_acquire):
       whichever block is listed first creates the unit, the rest adopt it. */
    esp_err_t {{prefix_lc}}_err = zc_adc_unit_acquire((adc_unit_t){{prefix}}_ADC_UNIT, &s_{{prefix_lc}}_adc);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit {{prefix_lc}} (unit=%d): %s", {{prefix}}_ADC_UNIT, esp_err_to_name({{prefix_lc}}_err));
    } else {
        ESP_LOGI(TAG, "ADC unit {{prefix_lc}} initialized (unit=%d)", {{prefix}}_ADC_UNIT);
    }
}
