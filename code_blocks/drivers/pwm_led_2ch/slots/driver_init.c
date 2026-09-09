{
    ledc_timer_config_t {{prefix_lc}}_tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = (ledc_timer_t){{prefix}}_LEDC_TIMER,
        .freq_hz = {{prefix}}_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&{{prefix_lc}}_tcfg));
    const int {{prefix_lc}}_gpios[2] = { {{prefix}}_WARM_GPIO, {{prefix}}_COOL_GPIO };
    const int {{prefix_lc}}_chans[2] = { {{prefix}}_WARM_CHANNEL, {{prefix}}_COOL_CHANNEL };
    for (int i = 0; i < 2; ++i) {
        ledc_channel_config_t {{prefix_lc}}_ccfg = {
            .gpio_num = {{prefix_lc}}_gpios[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t){{prefix_lc}}_chans[i],
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = (ledc_timer_t){{prefix}}_LEDC_TIMER,
            .duty = 0,
            .hpoint = 0,
            .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
            .flags = { .output_invert = 0 },
        };
        ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_ccfg));
    }
    ESP_LOGI(TAG, "PWM CCT {{prefix_lc}} initialized (warm GPIO %d / cool GPIO %d)", {{prefix}}_WARM_GPIO, {{prefix}}_COOL_GPIO);
}
