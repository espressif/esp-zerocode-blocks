case {{cfg.speed_param}}: {
    int16_t s = val.i16;
    bool fwd = (s > 0);
    bool rev = (s < 0);
    gpio_set_level((gpio_num_t){{prefix}}_HB_IN1_GPIO, fwd ? 1 : 0);
    gpio_set_level((gpio_num_t){{prefix}}_HB_IN2_GPIO, rev ? 1 : 0);
    uint32_t mag = (s < 0) ? (uint32_t)(-s) : (uint32_t)s;
    if (mag > 1000) mag = 1000;
    uint32_t duty = (mag * 1023U) / 1000U;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_HB_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_HB_CHANNEL);
    return ESP_OK;
}
