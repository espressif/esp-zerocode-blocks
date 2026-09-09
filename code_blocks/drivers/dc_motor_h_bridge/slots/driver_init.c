{
    gpio_config_t {{prefix_lc}}_in_cfg = {
        .pin_bit_mask = (1ULL << {{prefix}}_HB_IN1_GPIO) | (1ULL << {{prefix}}_HB_IN2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&{{prefix_lc}}_in_cfg);
    gpio_set_level((gpio_num_t){{prefix}}_HB_IN1_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_HB_IN2_GPIO, 0);
    ledc_timer_config_t {{prefix_lc}}_tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = (ledc_timer_t){{prefix}}_HB_TIMER,
        .freq_hz = {{prefix}}_HB_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&{{prefix_lc}}_tcfg));
    ledc_channel_config_t {{prefix_lc}}_ccfg = {
        .gpio_num = {{prefix}}_HB_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t){{prefix}}_HB_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t){{prefix}}_HB_TIMER,
        .duty = 0, .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = { .output_invert = 0 },
    };
    ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_ccfg));
    ESP_LOGI(TAG, "H-bridge {{prefix_lc}}: in1=%d in2=%d pwm=%d", {{prefix}}_HB_IN1_GPIO, {{prefix}}_HB_IN2_GPIO, {{prefix}}_HB_PWM_GPIO);
}
