{
    ledc_timer_config_t {{prefix_lc}}_buz_tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = (ledc_timer_t){{prefix}}_BUZZER_TIMER,
        .freq_hz = 4000,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&{{prefix_lc}}_buz_tcfg));
    ledc_channel_config_t {{prefix_lc}}_buz_ccfg = {
        .gpio_num = {{prefix}}_BUZZER_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t){{prefix}}_BUZZER_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t){{prefix}}_BUZZER_TIMER,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = { .output_invert = 0 },
    };
    ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_buz_ccfg));
    ESP_LOGI(TAG, "Buzzer {{prefix_lc}} initialized (GPIO %d)", {{prefix}}_BUZZER_GPIO);
}
