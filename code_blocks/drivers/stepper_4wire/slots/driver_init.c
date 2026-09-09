{
    gpio_config_t {{prefix_lc}}_st_cfg = {
        .pin_bit_mask = (1ULL << {{prefix}}_ST_IN1) | (1ULL << {{prefix}}_ST_IN2)
                      | (1ULL << {{prefix}}_ST_IN3) | (1ULL << {{prefix}}_ST_IN4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&{{prefix_lc}}_st_cfg);
    xTaskCreate({{prefix_lc}}_step_task, "{{prefix_lc}}_st", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Stepper {{prefix_lc}}: %d/%d/%d/%d", {{prefix}}_ST_IN1, {{prefix}}_ST_IN2, {{prefix}}_ST_IN3, {{prefix}}_ST_IN4);
}
