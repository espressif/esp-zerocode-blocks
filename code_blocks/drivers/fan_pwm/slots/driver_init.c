{
    ledc_timer_config_t {{prefix_lc}}_fan_timer_cfg = {};
    {{prefix_lc}}_fan_timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_fan_timer_cfg.timer_num = (ledc_timer_t){{prefix}}_FAN_LEDC_TIMER;
    {{prefix_lc}}_fan_timer_cfg.freq_hz = {{prefix}}_FAN_FREQ_HZ;
    {{prefix_lc}}_fan_timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    {{prefix_lc}}_fan_timer_cfg.deconfigure = false;
    /* Highest duty resolution the clock reaches at this frequency. */
    uint32_t {{prefix_lc}}_fan_max_duty = 0;
    if (app_utils_ledc_timer_config(&{{prefix_lc}}_fan_timer_cfg, &{{prefix_lc}}_fan_max_duty) != ESP_OK) {
        ESP_LOGE(TAG, "Fan PWM {{prefix_lc}}: %d Hz is not reachable, fan disabled", {{prefix}}_FAN_FREQ_HZ);
        return;
    }
    ledc_channel_config_t {{prefix_lc}}_fan_chan_cfg = {};
    {{prefix_lc}}_fan_chan_cfg.gpio_num = {{prefix}}_FAN_GPIO;
    {{prefix_lc}}_fan_chan_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_fan_chan_cfg.channel = (ledc_channel_t){{prefix}}_FAN_LEDC_CHANNEL;
    {{prefix_lc}}_fan_chan_cfg.timer_sel = (ledc_timer_t){{prefix}}_FAN_LEDC_TIMER;
    {{prefix_lc}}_fan_chan_cfg.duty = 0;
    {{prefix_lc}}_fan_chan_cfg.hpoint = 0;
    {{prefix_lc}}_fan_chan_cfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    {{prefix_lc}}_fan_chan_cfg.flags.output_invert = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_fan_chan_cfg));
    s_{{prefix_lc}}_fan_max_duty = {{prefix_lc}}_fan_max_duty;
    ESP_LOGI(TAG, "Fan PWM {{prefix_lc}} initialized (GPIO %d ch=%d, %d Hz, %d-bit)", {{prefix}}_FAN_GPIO, {{prefix}}_FAN_LEDC_CHANNEL,
             {{prefix}}_FAN_FREQ_HZ, (int){{prefix_lc}}_fan_timer_cfg.duty_resolution);
}
