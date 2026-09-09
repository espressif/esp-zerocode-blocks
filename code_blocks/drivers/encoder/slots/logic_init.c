#if SOC_PCNT_SUPPORTED
{
    pcnt_unit_config_t {{prefix_lc}}_unit_cfg = {
        .low_limit = -1000,
        .high_limit = 1000,
        .flags = { .accum_count = 0 },
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&{{prefix_lc}}_unit_cfg, &s_{{prefix_lc}}_pcnt));

    pcnt_chan_config_t {{prefix_lc}}_chan_cfg = {
        .edge_gpio_num = {{prefix}}_ENC_A_GPIO,
        .level_gpio_num = {{prefix}}_ENC_B_GPIO,
        /* ESP-IDF 6 removed `io_loop_back` from EVERY driver's config flags: binding
           two drivers to the same GPIO now just works, so the flag had nothing left
           to do. Setting it is a hard compile error, not a deprecation. */
        .flags = { .invert_edge_input = 0, .invert_level_input = 0, .virt_edge_io_level = 0, .virt_level_io_level = 0 },
    };
    pcnt_channel_handle_t {{prefix_lc}}_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(s_{{prefix_lc}}_pcnt, &{{prefix_lc}}_chan_cfg, &{{prefix_lc}}_chan));
    pcnt_channel_set_edge_action({{prefix_lc}}_chan, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action({{prefix_lc}}_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    pcnt_unit_enable(s_{{prefix_lc}}_pcnt);
    pcnt_unit_clear_count(s_{{prefix_lc}}_pcnt);
    pcnt_unit_start(s_{{prefix_lc}}_pcnt);

    const esp_timer_create_args_t {{prefix_lc}}_args = {
        .callback = &{{prefix_lc}}_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_enc",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, 50000ULL)); /* 50 ms */
}
#else /* !SOC_PCNT_SUPPORTED — GPIO-ISR quadrature fallback */
{
    gpio_config_t {{prefix_lc}}_a_cfg = {
        .pin_bit_mask = 1ULL << {{prefix}}_ENC_A_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_a_cfg));
    gpio_config_t {{prefix_lc}}_b_cfg = {
        .pin_bit_mask = 1ULL << {{prefix}}_ENC_B_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_b_cfg));

    /* Another block (buttons etc.) may already have installed the shared GPIO
     * ISR service — INVALID_STATE is the "already installed" answer, not an
     * error (same tolerance as peripherals/gpio_isr_service). */
    esp_err_t {{prefix_lc}}_isr_err = gpio_install_isr_service(0);
    if ({{prefix_lc}}_isr_err != ESP_OK && {{prefix_lc}}_isr_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK({{prefix_lc}}_isr_err);
    }
    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t){{prefix}}_ENC_A_GPIO, {{prefix_lc}}_a_isr, NULL));

    const esp_timer_create_args_t {{prefix_lc}}_args = {
        .callback = &{{prefix_lc}}_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_enc",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, 50000ULL)); /* 50 ms */
    ESP_LOGI("{{prefix_lc}}_enc", "GPIO-ISR encoder fallback active (no PCNT on this target)");
}
#endif /* SOC_PCNT_SUPPORTED */
