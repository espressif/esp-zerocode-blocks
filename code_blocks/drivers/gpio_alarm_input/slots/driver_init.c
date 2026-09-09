{
    gpio_config_t {{prefix_lc}}_alarm_conf = {
        .pin_bit_mask = (1ULL << {{prefix}}_ALARM_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = ({{prefix}}_ALARM_ACTIVE_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = ({{prefix}}_ALARM_ACTIVE_LEVEL == 1) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_alarm_conf));
    ESP_LOGI(TAG, "Alarm input {{prefix_lc}} configured (GPIO %d, active=%d)", {{prefix}}_ALARM_GPIO, {{prefix}}_ALARM_ACTIVE_LEVEL);
}
