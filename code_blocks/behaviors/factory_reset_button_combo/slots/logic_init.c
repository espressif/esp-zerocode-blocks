{
    gpio_config_t {{prefix_lc}}_in = {
        .pin_bit_mask = (1ULL << {{prefix}}_FR_GPIO_A) | (1ULL << {{prefix}}_FR_GPIO_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = ({{prefix}}_FR_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = ({{prefix}}_FR_LEVEL == 1) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_in));
    const esp_timer_create_args_t {{prefix_lc}}_args = {
        .callback = &{{prefix_lc}}_combo_poll,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_combo",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, 100000ULL)); /* 100 ms */
    ESP_LOGI(TAG, "{{prefix_lc}}: combo factory-reset armed (GPIO %d + %d, %u ms)", {{prefix}}_FR_GPIO_A, {{prefix}}_FR_GPIO_B, {{prefix}}_FR_HOLD_MS);
}
