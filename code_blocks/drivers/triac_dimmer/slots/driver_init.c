{
    gpio_config_t {{prefix_lc}}_gate_cfg = {
        .pin_bit_mask = (1ULL << {{prefix}}_TRIAC_GATE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&{{prefix_lc}}_gate_cfg);
    gpio_set_level((gpio_num_t){{prefix}}_TRIAC_GATE_GPIO, 0);
    gpio_config_t {{prefix_lc}}_zcd_cfg = {
        .pin_bit_mask = (1ULL << {{prefix}}_TRIAC_ZCD_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&{{prefix_lc}}_zcd_cfg);
    const esp_timer_create_args_t {{prefix_lc}}_gate_args = {
        .callback = &{{prefix_lc}}_gate_fire,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_ISR,
        .name = "{{prefix_lc}}_gate",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_gate_args, &s_{{prefix_lc}}_gate_timer));
    gpio_isr_handler_add((gpio_num_t){{prefix}}_TRIAC_ZCD_GPIO, {{prefix_lc}}_zcd_isr, NULL);
    ESP_LOGI(TAG, "Triac dimmer {{prefix_lc}}: ZCD=%d gate=%d", {{prefix}}_TRIAC_ZCD_GPIO, {{prefix}}_TRIAC_GATE_GPIO);
}
