{
    ledc_timer_config_t {{prefix_lc}}_servo_tcfg = {};
    {{prefix_lc}}_servo_tcfg.speed_mode = LEDC_LOW_SPEED_MODE;
    /* 14-bit: ESP32-C3 LEDC max is 14-bit (SOC_LEDC_TIMER_BIT_WIDTH); valid on all chips. */
    {{prefix_lc}}_servo_tcfg.duty_resolution = LEDC_TIMER_14_BIT;
    {{prefix_lc}}_servo_tcfg.timer_num = (ledc_timer_t){{prefix}}_SERVO_TIMER;
    {{prefix_lc}}_servo_tcfg.freq_hz = 50;
    {{prefix_lc}}_servo_tcfg.clk_cfg = LEDC_AUTO_CLK;
    {{prefix_lc}}_servo_tcfg.deconfigure = false;
    ESP_ERROR_CHECK(ledc_timer_config(&{{prefix_lc}}_servo_tcfg));
    ledc_channel_config_t {{prefix_lc}}_servo_ccfg = {};
    {{prefix_lc}}_servo_ccfg.gpio_num = {{prefix}}_SERVO_GPIO;
    {{prefix_lc}}_servo_ccfg.speed_mode = LEDC_LOW_SPEED_MODE;
    {{prefix_lc}}_servo_ccfg.channel = (ledc_channel_t){{prefix}}_SERVO_CHANNEL;
    {{prefix_lc}}_servo_ccfg.timer_sel = (ledc_timer_t){{prefix}}_SERVO_TIMER;
    /* The channel drives the pin the instant it is configured, so this duty
     * IS the boot position — it cannot be corrected after the fact. Derived
     * from the block's own rest_deg (closed/off by default) rather than the
     * middle of travel. */
    {{prefix_lc}}_servo_ccfg.duty = {{prefix}}_SERVO_DUTY_FOR_DEG({{prefix}}_SERVO_REST_DEG);
    {{prefix_lc}}_servo_ccfg.hpoint = 0;
    {{prefix_lc}}_servo_ccfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    {{prefix_lc}}_servo_ccfg.flags.output_invert = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&{{prefix_lc}}_servo_ccfg));
    ESP_LOGI(TAG, "Servo {{prefix_lc}} initialized (GPIO %d, resting at %d deg)",
             {{prefix}}_SERVO_GPIO, {{prefix}}_SERVO_REST_DEG);
}
