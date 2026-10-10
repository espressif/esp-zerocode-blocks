{
    gpio_config_t {{prefix_lc}}_in_conf = {};
    {{prefix_lc}}_in_conf.pin_bit_mask = (1ULL << {{prefix}}_INPUT_GPIO);
    {{prefix_lc}}_in_conf.mode = GPIO_MODE_INPUT;
    {{prefix_lc}}_in_conf.pull_up_en = ({{prefix}}_INPUT_ACTIVE_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_in_conf.pull_down_en = ({{prefix}}_INPUT_ACTIVE_LEVEL == 1) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_in_conf.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_in_conf));
    ESP_LOGI(TAG, "GPIO input {{prefix_lc}} configured (GPIO %d, active=%d)", {{prefix}}_INPUT_GPIO, {{prefix}}_INPUT_ACTIVE_LEVEL);
}
