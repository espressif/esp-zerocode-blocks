{
    ledc_timer_config_t {{prefix_lc}}_tcfg = {};
    {{prefix_lc}}_tcfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_tcfg.duty_resolution = LEDC_TIMER_13_BIT;
    {{prefix_lc}}_tcfg.timer_num = (ledc_timer_t){{prefix}}_LEDC_TIMER;
    {{prefix_lc}}_tcfg.freq_hz = {{prefix}}_FREQ_HZ;
    {{prefix_lc}}_tcfg.clk_cfg = LEDC_AUTO_CLK;
    {{prefix_lc}}_tcfg.deconfigure = false;
    ESP_ERROR_CHECK(ledc_timer_config(&{{prefix_lc}}_tcfg));
    const int {{prefix_lc}}_gpios[3] = { {{prefix}}_R_GPIO, {{prefix}}_G_GPIO, {{prefix}}_B_GPIO };
    const int {{prefix_lc}}_chans[3] = { {{prefix}}_R_CH, {{prefix}}_G_CH, {{prefix}}_B_CH };
    for (int i = 0; i < 3; ++i) {
        ledc_channel_config_t {{prefix_lc}}_ccfg = {};
        {{prefix_lc}}_ccfg.gpio_num = {{prefix_lc}}_gpios[i];
        {{prefix_lc}}_ccfg.speed_mode = LEDC_LOW_SPEED_MODE;
        {{prefix_lc}}_ccfg.channel = (ledc_channel_t){{prefix_lc}}_chans[i];
        {{prefix_lc}}_ccfg.timer_sel = (ledc_timer_t){{prefix}}_LEDC_TIMER;
        {{prefix_lc}}_ccfg.duty = 0;
        {{prefix_lc}}_ccfg.hpoint = 0;
        {{prefix_lc}}_ccfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
        {{prefix_lc}}_ccfg.flags.output_invert = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_ccfg));
    }
    ESP_LOGI(TAG, "PWM RGB {{prefix_lc}} initialized");
}
