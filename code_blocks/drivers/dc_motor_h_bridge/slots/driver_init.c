{
    gpio_config_t {{prefix_lc}}_in_cfg = {};
    {{prefix_lc}}_in_cfg.pin_bit_mask = (1ULL << {{prefix}}_HB_IN1_GPIO) | (1ULL << {{prefix}}_HB_IN2_GPIO);
    {{prefix_lc}}_in_cfg.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_in_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_in_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_in_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&{{prefix_lc}}_in_cfg);
    gpio_set_level((gpio_num_t){{prefix}}_HB_IN1_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_HB_IN2_GPIO, 0);
    ledc_timer_config_t {{prefix_lc}}_tcfg = {};
    {{prefix_lc}}_tcfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_tcfg.timer_num = (ledc_timer_t){{prefix}}_HB_TIMER;
    {{prefix_lc}}_tcfg.freq_hz = {{prefix}}_HB_FREQ_HZ;
    {{prefix_lc}}_tcfg.clk_cfg = LEDC_AUTO_CLK;
    {{prefix_lc}}_tcfg.deconfigure = false;
    /* Highest duty resolution the clock reaches at this frequency. */
    uint32_t {{prefix_lc}}_hb_max_duty = 0;
    if (app_utils_ledc_timer_config(&{{prefix_lc}}_tcfg, &{{prefix_lc}}_hb_max_duty) != ESP_OK) {
        ESP_LOGE(TAG, "H-bridge {{prefix_lc}}: %d Hz is not reachable, motor disabled (coasting)", {{prefix}}_HB_FREQ_HZ);
        return;
    }
    ledc_channel_config_t {{prefix_lc}}_ccfg = {};
    {{prefix_lc}}_ccfg.gpio_num = {{prefix}}_HB_PWM_GPIO;
    {{prefix_lc}}_ccfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_ccfg.channel = (ledc_channel_t){{prefix}}_HB_CHANNEL;
    {{prefix_lc}}_ccfg.timer_sel = (ledc_timer_t){{prefix}}_HB_TIMER;
    {{prefix_lc}}_ccfg.duty = 0;
    {{prefix_lc}}_ccfg.hpoint = 0;
    {{prefix_lc}}_ccfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    {{prefix_lc}}_ccfg.flags.output_invert = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_ccfg));
    s_{{prefix_lc}}_hb_max_duty = {{prefix_lc}}_hb_max_duty;
    ESP_LOGI(TAG, "H-bridge {{prefix_lc}}: in1=%d in2=%d pwm=%d (%d Hz, %d-bit)", {{prefix}}_HB_IN1_GPIO, {{prefix}}_HB_IN2_GPIO, {{prefix}}_HB_PWM_GPIO,
             {{prefix}}_HB_FREQ_HZ, (int){{prefix_lc}}_tcfg.duty_resolution);
}
