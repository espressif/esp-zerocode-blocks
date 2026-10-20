{
    /* All coils off BEFORE output mode (see "Actuators must be born safe"). */
    {{prefix_lc}}_coils_off();
    gpio_config_t {{prefix_lc}}_st_cfg = {};
    {{prefix_lc}}_st_cfg.pin_bit_mask = (1ULL << {{prefix}}_ST_IN1) | (1ULL << {{prefix}}_ST_IN2) | (1ULL << {{prefix}}_ST_IN3) | (1ULL << {{prefix}}_ST_IN4);
    {{prefix_lc}}_st_cfg.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_st_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_st_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_st_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&{{prefix_lc}}_st_cfg);
    {{prefix_lc}}_coils_off();
    const esp_timer_create_args_t {{prefix_lc}}_st_args = {
        .callback = &{{prefix_lc}}_step_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_st",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_st_args, &s_{{prefix_lc}}_step_timer));
    ESP_LOGI(TAG, "Stepper {{prefix_lc}}: %d/%d/%d/%d", {{prefix}}_ST_IN1, {{prefix}}_ST_IN2, {{prefix}}_ST_IN3, {{prefix}}_ST_IN4);
}
