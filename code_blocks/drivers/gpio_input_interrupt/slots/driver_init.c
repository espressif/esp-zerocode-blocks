{
    gpio_config_t {{prefix_lc}}_in_cfg = {};
    {{prefix_lc}}_in_cfg.pin_bit_mask = (1ULL << {{prefix}}_INTR_GPIO);
    {{prefix_lc}}_in_cfg.mode = GPIO_MODE_INPUT;
    /* cfg.pull wins when named; auto pulls toward the inactive (not pressed) level. */
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
    {{prefix_lc}}_in_cfg.pull_up_en = ({{prefix}}_INTR_PRESS_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_in_cfg.pull_down_en = ({{prefix}}_INTR_PRESS_LEVEL == 1) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
#endif
    /* Both edges in every mode: the param is a level, and a level that only
       sees its press edge latches true (the release is never read).
       cfg.intr_type decides what is PUBLISHED — see {{prefix_lc}}_intr_publish. */
    {{prefix_lc}}_in_cfg.intr_type = GPIO_INTR_ANYEDGE;
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
            ESP_LOGI(TAG, "Interrupt input {{prefix_lc}}: GPIO %d, %s, pressed=%d", {{prefix}}_INTR_GPIO,
                     {{prefix}}_INTR_MOMENTARY ? "momentary (press = event)" : "level (state)", {{prefix}}_INTR_PRESS_LEVEL);
#if {{prefix}}_INTR_MOMENTARY
            if ({{prefix}}_INTR_PRESS_LEVEL != {{prefix}}_INTR_ACTIVE_LEVEL) {
                ESP_LOGW(TAG, "Interrupt input {{prefix_lc}}: active_level %d disagrees with {{cfg.intr_type}}; the edge decides (pressed=%d)",
                         {{prefix}}_INTR_ACTIVE_LEVEL, {{prefix}}_INTR_PRESS_LEVEL);
            }
#endif
        }
    }

    /* The level at boot is the state until the first edge — publish it, or a
       contact already open at power-up reads "closed" until it moves. AFTER
       the handler is attached, so an edge in between is queued, not lost. */
    {{prefix_lc}}_intr_publish();
}
