{
    gpio_config_t {{prefix_lc}}_gate_cfg = {};
    {{prefix_lc}}_gate_cfg.pin_bit_mask = (1ULL << {{prefix}}_TRIAC_GATE_GPIO);
    {{prefix_lc}}_gate_cfg.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_gate_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_gate_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_gate_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&{{prefix_lc}}_gate_cfg);
    gpio_set_level((gpio_num_t){{prefix}}_TRIAC_GATE_GPIO, 0);
    gpio_config_t {{prefix_lc}}_zcd_cfg = {};
    {{prefix_lc}}_zcd_cfg.pin_bit_mask = (1ULL << {{prefix}}_TRIAC_ZCD_GPIO);
    {{prefix_lc}}_zcd_cfg.mode = GPIO_MODE_INPUT;
    {{prefix_lc}}_zcd_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_zcd_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_zcd_cfg.intr_type = GPIO_INTR_POSEDGE;
    gpio_config(&{{prefix_lc}}_zcd_cfg);
    const esp_timer_create_args_t {{prefix_lc}}_gate_args = {
        .callback = &{{prefix_lc}}_gate_fire,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_ISR,
        .name = "{{prefix_lc}}_gate",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_gate_args, &s_{{prefix_lc}}_gate_timer));
    /* Adopt-or-install the shared GPIO ISR service (drivers/encoder does the
       same): INVALID_STATE is "already installed", not an error. Relying on
       peripherals/gpio_isr_service being listed ABOVE this block was an
       unchecked assumption — without it gpio_isr_handler_add failed, its
       result was ignored, and the dimmer silently never fired. */
    esp_err_t {{prefix_lc}}_err = gpio_install_isr_service(0);
    if ({{prefix_lc}}_err == ESP_ERR_INVALID_STATE) {{prefix_lc}}_err = ESP_OK;
    if ({{prefix_lc}}_err == ESP_OK) {
        {{prefix_lc}}_err = gpio_isr_handler_add((gpio_num_t){{prefix}}_TRIAC_ZCD_GPIO, {{prefix_lc}}_zcd_isr, NULL);
    }
    if ({{prefix_lc}}_err != ESP_OK) {
        /* Fail closed: the gate is held low, so the load stays OFF. */
        ESP_LOGE(TAG, "Triac dimmer {{prefix_lc}}: ZCD interrupt on GPIO %d unavailable (%s) — dimmer disabled",
                 {{prefix}}_TRIAC_ZCD_GPIO, esp_err_to_name({{prefix_lc}}_err));
    } else {
        ESP_LOGI(TAG, "Triac dimmer {{prefix_lc}}: ZCD=%d gate=%d", {{prefix}}_TRIAC_ZCD_GPIO, {{prefix}}_TRIAC_GATE_GPIO);
    }
}
