{
    gpio_config_t {{prefix_lc}}_out = {};
    {{prefix_lc}}_out.pin_bit_mask = (1ULL << {{prefix}}_UP_GPIO) | (1ULL << {{prefix}}_DOWN_GPIO);
    {{prefix_lc}}_out.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_out.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_out.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_out.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_out));
    {{prefix_lc}}_motor_off();

    gpio_config_t {{prefix_lc}}_in = {};
    {{prefix_lc}}_in.pin_bit_mask = (1ULL << {{prefix}}_UPPER_LIMIT_GPIO) | (1ULL << {{prefix}}_LOWER_LIMIT_GPIO);
    {{prefix_lc}}_in.mode = GPIO_MODE_INPUT;
    {{prefix_lc}}_in.pull_up_en = GPIO_PULLUP_ENABLE;
    {{prefix_lc}}_in.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_in.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_in));
    ESP_LOGI(TAG, "Curtain motor {{prefix_lc}} initialized");
}
