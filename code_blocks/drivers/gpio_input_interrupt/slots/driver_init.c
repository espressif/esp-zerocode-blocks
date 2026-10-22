{
    gpio_config_t {{prefix_lc}}_in_cfg = {};
    {{prefix_lc}}_in_cfg.pin_bit_mask = (1ULL << {{prefix}}_INTR_GPIO);
    {{prefix_lc}}_in_cfg.mode = GPIO_MODE_INPUT;
    /* cfg.pull wins when named; auto pulls toward the inactive level. */
#if defined({{prefix}}_INTR_PULL_pullup)
    {{prefix_lc}}_in_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    {{prefix_lc}}_in_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
#elif defined({{prefix}}_INTR_PULL_pulldown)
    {{prefix_lc}}_in_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_in_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
#elif defined({{prefix}}_INTR_PULL_none)
    {{prefix_lc}}_in_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_in_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
#else
    {{prefix_lc}}_in_cfg.pull_up_en = ({{prefix}}_INTR_ACTIVE_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_in_cfg.pull_down_en = ({{prefix}}_INTR_ACTIVE_LEVEL == 1) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
#endif
    {{prefix_lc}}_in_cfg.intr_type = {{cfg.intr_type}};
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_in_cfg));
    s_{{prefix_lc}}_intr_q = xQueueCreate(8, sizeof(uint32_t));
    xTaskCreate({{prefix_lc}}_intr_task, "{{prefix_lc}}_intr", 2048, NULL, 5, NULL);

    /* Install the shared ISR service ourselves (INVALID_STATE = already
       installed, by peripherals/gpio_isr_service or another driver). Relying
       on a peripheral nothing declared or ordered left the handler_add below
       failing silently and the sensor dead. */
    esp_err_t {{prefix_lc}}_isr_err = gpio_install_isr_service(0);
    if ({{prefix_lc}}_isr_err != ESP_OK && {{prefix_lc}}_isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Interrupt input {{prefix_lc}}: ISR service install failed (%s) — edges will not be seen",
                 esp_err_to_name({{prefix_lc}}_isr_err));
    } else {
        {{prefix_lc}}_isr_err = gpio_isr_handler_add((gpio_num_t){{prefix}}_INTR_GPIO, {{prefix_lc}}_intr_isr, NULL);
        if ({{prefix_lc}}_isr_err != ESP_OK) {
            ESP_LOGE(TAG, "Interrupt input {{prefix_lc}}: gpio_isr_handler_add(GPIO %d) failed (%s) — edges will not be seen",
                     {{prefix}}_INTR_GPIO, esp_err_to_name({{prefix_lc}}_isr_err));
        } else {
            ESP_LOGI(TAG, "Interrupt input {{prefix_lc}}: GPIO %d", {{prefix}}_INTR_GPIO);
        }
    }

    /* The level at boot is the state until the first edge — publish it, or a
       contact already open at power-up reads "closed" until it moves. AFTER
       the handler is attached, so an edge in between is queued, not lost. */
    {{prefix_lc}}_intr_publish();
}
