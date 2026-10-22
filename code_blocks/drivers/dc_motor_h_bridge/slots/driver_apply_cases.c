case {{cfg.speed_param}}: {
    if (s_{{prefix_lc}}_hb_max_duty == 0) {
        return ESP_OK; /* init could not reach the frequency, and said so: stay coasting */
    }
    int16_t s = val.i16;
    bool fwd = (s > 0);
    bool rev = (s < 0);
    gpio_set_level((gpio_num_t){{prefix}}_HB_IN1_GPIO, fwd ? 1 : 0);
    gpio_set_level((gpio_num_t){{prefix}}_HB_IN2_GPIO, rev ? 1 : 0);
    uint32_t mag = (s < 0) ? (uint32_t)(-s) : (uint32_t)s;
    if (mag > 1000) mag = 1000;
    uint32_t duty = (uint32_t)(((uint64_t)mag * s_{{prefix_lc}}_hb_max_duty) / 1000U);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_HB_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_HB_CHANNEL);
    return ESP_OK;
}
