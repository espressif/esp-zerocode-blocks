{
#if defined({{prefix}}_BUZZER_KIND_active)
    /* Active buzzer: a plain output, silent from the first instant (the level
     * is latched before the pin becomes an output). */
    gpio_set_level((gpio_num_t){{prefix}}_BUZZER_GPIO, !{{prefix}}_BUZZER_ACTIVE_LEVEL);
    gpio_config_t {{prefix_lc}}_buz_gcfg = {};
    {{prefix_lc}}_buz_gcfg.pin_bit_mask = 1ULL << {{prefix}}_BUZZER_GPIO;
    {{prefix_lc}}_buz_gcfg.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_buz_gcfg.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_buz_gcfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_buz_gcfg.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_buz_gcfg));
    gpio_set_level((gpio_num_t){{prefix}}_BUZZER_GPIO, !{{prefix}}_BUZZER_ACTIVE_LEVEL);
    ESP_LOGI(TAG, "Buzzer {{prefix_lc}} initialized (active, GPIO %d, on = %d)",
             {{prefix}}_BUZZER_GPIO, {{prefix}}_BUZZER_ACTIVE_LEVEL);
#else
    ledc_timer_config_t {{prefix_lc}}_buz_tcfg = {};
    {{prefix_lc}}_buz_tcfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_buz_tcfg.duty_resolution = (ledc_timer_bit_t){{prefix}}_BUZZER_DUTY_BITS;
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
    ESP_LOGI(TAG, "Buzzer {{prefix_lc}} initialized (passive, GPIO %d)", {{prefix}}_BUZZER_GPIO);
#endif
}
