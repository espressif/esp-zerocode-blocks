{
    /* Idle level BEFORE output mode (see "Actuators must be born safe"). */
    gpio_set_level((gpio_num_t){{prefix}}_LOCK_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_UNLOCK_GPIO, 0);
    gpio_config_t {{prefix_lc}}_out = {};
    {{prefix_lc}}_out.pin_bit_mask = (1ULL << {{prefix}}_LOCK_GPIO) | (1ULL << {{prefix}}_UNLOCK_GPIO);
    {{prefix_lc}}_out.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_out.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_out.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_out.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_out));
    gpio_set_level((gpio_num_t){{prefix}}_LOCK_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_UNLOCK_GPIO, 0);

    /* Pulled toward the UNLOCKED reading: an unwired sense switch must report
       unlocked, never locked. */
    gpio_config_t {{prefix_lc}}_in = {};
    {{prefix_lc}}_in.pin_bit_mask = (1ULL << {{prefix}}_POSITION_GPIO);
    {{prefix_lc}}_in.mode = GPIO_MODE_INPUT;
    {{prefix_lc}}_in.pull_up_en = {{prefix}}_POSITION_ACTIVE ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE;
    {{prefix_lc}}_in.pull_down_en = {{prefix}}_POSITION_ACTIVE ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_in.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_in));

    const esp_timer_create_args_t {{prefix_lc}}_pulse_args = {
        .callback = &{{prefix_lc}}_pulse_end_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_pulse",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_pulse_args, &s_{{prefix_lc}}_pulse_timer));

    const esp_timer_create_args_t {{prefix_lc}}_poll_args = {
        .callback = &{{prefix_lc}}_position_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_pos",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_poll = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_poll_args, &{{prefix_lc}}_poll));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_poll, 250000ULL)); /* 250 ms */
    ESP_LOGI(TAG, "Lock motor {{prefix_lc}}: lock=%d unlock=%d pos=%d (locked reads %d)",
             {{prefix}}_LOCK_GPIO, {{prefix}}_UNLOCK_GPIO, {{prefix}}_POSITION_GPIO, {{prefix}}_POSITION_ACTIVE);
}
