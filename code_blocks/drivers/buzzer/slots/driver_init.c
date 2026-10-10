{
    ledc_timer_config_t {{prefix_lc}}_buz_tcfg = {};
    {{prefix_lc}}_buz_tcfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_buz_tcfg.duty_resolution = LEDC_TIMER_8_BIT;
    {{prefix_lc}}_buz_tcfg.timer_num = (ledc_timer_t){{prefix}}_BUZZER_TIMER;
    {{prefix_lc}}_buz_tcfg.freq_hz = 4000;
    {{prefix_lc}}_buz_tcfg.clk_cfg = LEDC_AUTO_CLK;
    {{prefix_lc}}_buz_tcfg.deconfigure = false;
    ESP_ERROR_CHECK(ledc_timer_config(&{{prefix_lc}}_buz_tcfg));
    ledc_channel_config_t {{prefix_lc}}_buz_ccfg = {};
    {{prefix_lc}}_buz_ccfg.gpio_num = {{prefix}}_BUZZER_GPIO;
    {{prefix_lc}}_buz_ccfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_buz_ccfg.channel = (ledc_channel_t){{prefix}}_BUZZER_CHANNEL;
    {{prefix_lc}}_buz_ccfg.timer_sel = (ledc_timer_t){{prefix}}_BUZZER_TIMER;
    {{prefix_lc}}_buz_ccfg.duty = 0;
    {{prefix_lc}}_buz_ccfg.hpoint = 0;
    {{prefix_lc}}_buz_ccfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    {{prefix_lc}}_buz_ccfg.flags.output_invert = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_buz_ccfg));
    ESP_LOGI(TAG, "Buzzer {{prefix_lc}} initialized (GPIO %d)", {{prefix}}_BUZZER_GPIO);
}
