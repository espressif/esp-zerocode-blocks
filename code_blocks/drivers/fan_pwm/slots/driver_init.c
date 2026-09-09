{
    ledc_timer_config_t {{prefix_lc}}_fan_timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = (ledc_timer_t){{prefix}}_FAN_LEDC_TIMER,
        .freq_hz = {{prefix}}_FAN_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&{{prefix_lc}}_fan_timer_cfg));
    ledc_channel_config_t {{prefix_lc}}_fan_chan_cfg = {
        .gpio_num = {{prefix}}_FAN_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t){{prefix}}_FAN_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t){{prefix}}_FAN_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = { .output_invert = 0 },
    };
    ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_fan_chan_cfg));
    ESP_LOGI(TAG, "Fan PWM {{prefix_lc}} initialized (GPIO %d ch=%d)", {{prefix}}_FAN_GPIO, {{prefix}}_FAN_LEDC_CHANNEL);
}
