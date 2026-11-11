{
    s_{{prefix_lc}}_dsc_deep_wake = esp_reset_reason() == ESP_RST_DEEPSLEEP;
    uint32_t {{prefix_lc}}_causes = esp_sleep_get_wakeup_causes();
    ESP_LOGI(TAG, "{{prefix_lc}}: %s (wake causes 0x%lx)",
             s_{{prefix_lc}}_dsc_deep_wake ? "woke from deep sleep" : "power-up or reset — staying awake for setup",
             (unsigned long){{prefix_lc}}_causes);
    for (int i = 0; i < 2; i++) {
        int pin = s_{{prefix_lc}}_dsc_pins[i];
        if (pin < 0) continue;
        if (!esp_sleep_is_valid_wakeup_gpio((gpio_num_t)pin)) {
            ESP_LOGE(TAG, "{{prefix_lc}}: GPIO %d cannot wake this chip from deep sleep — pick an RTC/LP pin", pin);
        }
        gpio_input_enable((gpio_num_t)pin);   /* readable even if no driver owns it */
    }
#if {{prefix}}_DSC_USB_AWAKE && !(SOC_USB_SERIAL_JTAG_SUPPORTED && CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED)
    ESP_LOGW(TAG, "{{prefix_lc}}: stay_awake_on_usb needs the chip's own USB console — ignored here");
#endif
    if (!{{prefix_lc}}_dsc_net_state) {
        ESP_LOGW(TAG, "{{prefix_lc}}: no delivery signal from this framework — sleeping after %d ms awake",
                 {{prefix}}_DSC_MAX_AWAKE_MS);
    }
    xTaskCreate({{prefix_lc}}_dsc_task, "{{prefix_lc}}_dsc", 4096, NULL, 2, NULL);
}
