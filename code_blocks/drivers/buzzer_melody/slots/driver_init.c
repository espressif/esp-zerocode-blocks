{
#if defined({{prefix}}_MEL_KIND_active)
    gpio_set_level((gpio_num_t){{prefix}}_MEL_GPIO, !{{prefix}}_MEL_ACTIVE_LEVEL);
    gpio_config_t {{prefix_lc}}_mgcfg = {};
    {{prefix_lc}}_mgcfg.pin_bit_mask = 1ULL << {{prefix}}_MEL_GPIO;
    {{prefix_lc}}_mgcfg.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_mgcfg.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_mgcfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_mgcfg.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_mgcfg));
    gpio_set_level((gpio_num_t){{prefix}}_MEL_GPIO, !{{prefix}}_MEL_ACTIVE_LEVEL);
#else
    ledc_timer_config_t {{prefix_lc}}_mtcfg = {};
    {{prefix_lc}}_mtcfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_mtcfg.duty_resolution = (ledc_timer_bit_t){{prefix}}_MEL_DUTY_BITS;
    {{prefix_lc}}_mtcfg.timer_num = (ledc_timer_t){{prefix}}_MEL_TIMER;
    {{prefix_lc}}_mtcfg.freq_hz = 4000;
    {{prefix_lc}}_mtcfg.clk_cfg = LEDC_AUTO_CLK;
    {{prefix_lc}}_mtcfg.deconfigure = false;
    ESP_ERROR_CHECK(ledc_timer_config(&{{prefix_lc}}_mtcfg));
    ledc_channel_config_t {{prefix_lc}}_mccfg = {};
    {{prefix_lc}}_mccfg.gpio_num = {{prefix}}_MEL_GPIO;
    {{prefix_lc}}_mccfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_mccfg.channel = (ledc_channel_t){{prefix}}_MEL_CHANNEL;
    {{prefix_lc}}_mccfg.timer_sel = (ledc_timer_t){{prefix}}_MEL_TIMER;
    {{prefix_lc}}_mccfg.duty = 0;
    {{prefix_lc}}_mccfg.hpoint = 0;
    {{prefix_lc}}_mccfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    {{prefix_lc}}_mccfg.flags.output_invert = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_mccfg));
#endif
    xTaskCreate({{prefix_lc}}_play_task, "{{prefix_lc}}_mel", 2048, NULL, 5, &s_{{prefix_lc}}_task);
    ESP_LOGI(TAG, "Buzzer melody {{prefix_lc}}: GPIO %d", {{prefix}}_MEL_GPIO);
}
