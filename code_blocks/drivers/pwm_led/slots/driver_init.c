{
    ledc_timer_config_t {{prefix_lc}}_timer_cfg = {};
    {{prefix_lc}}_timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_timer_cfg.duty_resolution = LEDC_TIMER_13_BIT;
    {{prefix_lc}}_timer_cfg.timer_num = (ledc_timer_t){{prefix}}_LED_LEDC_TIMER;
    {{prefix_lc}}_timer_cfg.freq_hz = {{prefix}}_LED_FREQ_HZ;
    {{prefix_lc}}_timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    {{prefix_lc}}_timer_cfg.deconfigure = false;
    ESP_ERROR_CHECK(ledc_timer_config(&{{prefix_lc}}_timer_cfg));
    ledc_channel_config_t {{prefix_lc}}_chan_cfg = {};
    {{prefix_lc}}_chan_cfg.gpio_num = {{prefix}}_LED_GPIO;
    {{prefix_lc}}_chan_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_chan_cfg.channel = (ledc_channel_t){{prefix}}_LED_LEDC_CHANNEL;
    {{prefix_lc}}_chan_cfg.timer_sel = (ledc_timer_t){{prefix}}_LED_LEDC_TIMER;
    {{prefix_lc}}_chan_cfg.duty = 0;
    {{prefix_lc}}_chan_cfg.hpoint = 0;
    {{prefix_lc}}_chan_cfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    {{prefix_lc}}_chan_cfg.flags.output_invert = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_chan_cfg));
    ESP_LOGI(TAG, "PWM LED {{prefix_lc}} initialized (GPIO %d ch=%d)", {{prefix}}_LED_GPIO, {{prefix}}_LED_LEDC_CHANNEL);
}
