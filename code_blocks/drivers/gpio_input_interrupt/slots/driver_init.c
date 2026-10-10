{
    gpio_config_t {{prefix_lc}}_in_cfg = {};
    {{prefix_lc}}_in_cfg.pin_bit_mask = (1ULL << {{prefix}}_INTR_GPIO);
    {{prefix_lc}}_in_cfg.mode = GPIO_MODE_INPUT;
    {{prefix_lc}}_in_cfg.pull_up_en = ({{prefix}}_INTR_ACTIVE_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_in_cfg.pull_down_en = ({{prefix}}_INTR_ACTIVE_LEVEL == 1) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_in_cfg.intr_type = {{cfg.intr_type}};
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_in_cfg));
    s_{{prefix_lc}}_intr_q = xQueueCreate(8, sizeof(uint32_t));
    xTaskCreate({{prefix_lc}}_intr_task, "{{prefix_lc}}_intr", 2048, NULL, 5, NULL);
    gpio_isr_handler_add((gpio_num_t){{prefix}}_INTR_GPIO, {{prefix_lc}}_intr_isr, NULL);
    ESP_LOGI(TAG, "Interrupt input {{prefix_lc}}: GPIO %d", {{prefix}}_INTR_GPIO);
}
