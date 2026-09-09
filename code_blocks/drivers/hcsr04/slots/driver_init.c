{
    gpio_config_t {{prefix_lc}}_trig_cfg = {
        .pin_bit_mask = (1ULL << {{prefix}}_HC_TRIG_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&{{prefix_lc}}_trig_cfg);
    gpio_set_level((gpio_num_t){{prefix}}_HC_TRIG_GPIO, 0);
    gpio_config_t {{prefix_lc}}_echo_cfg = {
        .pin_bit_mask = (1ULL << {{prefix}}_HC_ECHO_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&{{prefix_lc}}_echo_cfg);
    ESP_LOGI(TAG, "HC-SR04 {{prefix_lc}}: trig=%d echo=%d", {{prefix}}_HC_TRIG_GPIO, {{prefix}}_HC_ECHO_GPIO);
}
