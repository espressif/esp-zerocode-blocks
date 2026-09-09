{
    gpio_config_t {{prefix_lc}}_conf = {};
    {{prefix_lc}}_conf.pin_bit_mask = (1ULL << {{prefix}}_HAPTIC_GPIO);
    {{prefix_lc}}_conf.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_conf.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_conf));
    /* Idle BEFORE anything can trigger it — a motor that buzzes through boot
       is the haptic equivalent of a relay clacking on power-up. */
    gpio_set_level((gpio_num_t){{prefix}}_HAPTIC_GPIO, !{{prefix}}_HAPTIC_ACTIVE);

    const esp_timer_create_args_t {{prefix_lc}}_args = {
        .callback = &{{prefix_lc}}_off_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_haptic",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &s_{{prefix_lc}}_off_timer));
    ESP_LOGI(TAG, "Haptic {{prefix_lc}} initialized (GPIO %d, %d ms pulse)",
             {{prefix}}_HAPTIC_GPIO, {{prefix}}_HAPTIC_PULSE_MS);
}
