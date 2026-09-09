{
    esp_err_t {{prefix_lc}}_isr_err = gpio_install_isr_service({{cfg.intr_alloc_flags}});
    if ({{prefix_lc}}_isr_err != ESP_OK && {{prefix_lc}}_isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "GPIO ISR install ({{prefix_lc}}) returned %s", esp_err_to_name({{prefix_lc}}_isr_err));
    } else {
        ESP_LOGI(TAG, "GPIO ISR service ready ({{prefix_lc}})");
    }
}
