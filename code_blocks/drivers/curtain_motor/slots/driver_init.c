{
    /* Idle level BEFORE output mode (see "Actuators must be born safe"). */
    gpio_set_level((gpio_num_t){{prefix}}_UP_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_DOWN_GPIO, 0);
    gpio_config_t {{prefix_lc}}_out = {};
    {{prefix_lc}}_out.pin_bit_mask = (1ULL << {{prefix}}_UP_GPIO) | (1ULL << {{prefix}}_DOWN_GPIO);
    {{prefix_lc}}_out.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_out.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_out.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_out.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_out));
    {{prefix_lc}}_motor_off();

    /* Pull toward the INACTIVE level, so an unwired limit reads "not tripped"
       and max_run_ms stops the motor, rather than reading permanently tripped. */
    gpio_config_t {{prefix_lc}}_in = {};
    {{prefix_lc}}_in.pin_bit_mask = (1ULL << {{prefix}}_UPPER_LIMIT_GPIO) | (1ULL << {{prefix}}_LOWER_LIMIT_GPIO);
    {{prefix_lc}}_in.mode = GPIO_MODE_INPUT;
    {{prefix_lc}}_in.pull_up_en = {{prefix}}_LIMIT_ACTIVE ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE;
    {{prefix_lc}}_in.pull_down_en = {{prefix}}_LIMIT_ACTIVE ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_in.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_in));

    const esp_timer_create_args_t {{prefix_lc}}_poll_args = {
        .callback = &{{prefix_lc}}_limit_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_limits",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_poll = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_poll_args, &{{prefix_lc}}_poll));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_poll, 20000ULL)); /* 20 ms */
    ESP_LOGI(TAG, "Curtain motor {{prefix_lc}} initialized (limits active-%s, max run %d ms)",
             {{prefix}}_LIMIT_ACTIVE ? "high" : "low", {{prefix}}_MAX_RUN_MS);
}
