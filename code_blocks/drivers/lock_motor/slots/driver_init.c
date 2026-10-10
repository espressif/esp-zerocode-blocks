{
    gpio_config_t {{prefix_lc}}_out = {};
    {{prefix_lc}}_out.pin_bit_mask = (1ULL << {{prefix}}_LOCK_GPIO) | (1ULL << {{prefix}}_UNLOCK_GPIO);
    {{prefix_lc}}_out.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_out.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_out.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_out.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_out));
    gpio_set_level((gpio_num_t){{prefix}}_LOCK_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_UNLOCK_GPIO, 0);

    gpio_config_t {{prefix_lc}}_in = {};
    {{prefix_lc}}_in.pin_bit_mask = (1ULL << {{prefix}}_POSITION_GPIO);
    {{prefix_lc}}_in.mode = GPIO_MODE_INPUT;
    {{prefix_lc}}_in.pull_up_en = GPIO_PULLUP_ENABLE;
    {{prefix_lc}}_in.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_in.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_in));
    ESP_LOGI(TAG, "Lock motor {{prefix_lc}}: lock=%d unlock=%d pos=%d", {{prefix}}_LOCK_GPIO, {{prefix}}_UNLOCK_GPIO, {{prefix}}_POSITION_GPIO);
}
