{
    gpio_config_t {{prefix_lc}}_out = {
        .pin_bit_mask = (1ULL << {{prefix}}_UP_GPIO) | (1ULL << {{prefix}}_DOWN_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_out));
    {{prefix_lc}}_motor_off();

    gpio_config_t {{prefix_lc}}_in = {
        .pin_bit_mask = (1ULL << {{prefix}}_UPPER_LIMIT_GPIO) | (1ULL << {{prefix}}_LOWER_LIMIT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_in));
    ESP_LOGI(TAG, "Curtain motor {{prefix_lc}} initialized");
}
