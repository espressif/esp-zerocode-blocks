{
    gpio_config_t {{prefix_lc}}_out = {
        .pin_bit_mask = (1ULL << {{prefix}}_LOCK_GPIO) | (1ULL << {{prefix}}_UNLOCK_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_out));
    gpio_set_level((gpio_num_t){{prefix}}_LOCK_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_UNLOCK_GPIO, 0);

    gpio_config_t {{prefix_lc}}_in = {
        .pin_bit_mask = (1ULL << {{prefix}}_POSITION_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_in));
    ESP_LOGI(TAG, "Lock motor {{prefix_lc}}: lock=%d unlock=%d pos=%d", {{prefix}}_LOCK_GPIO, {{prefix}}_UNLOCK_GPIO, {{prefix}}_POSITION_GPIO);
}
