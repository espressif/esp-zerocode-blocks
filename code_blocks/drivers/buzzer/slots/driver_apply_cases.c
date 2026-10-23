case {{cfg.frequency_param}}: {
#if defined({{prefix}}_BUZZER_KIND_active)
    /* Active buzzer: its own oscillator sets the pitch — just power it. */
    gpio_set_level((gpio_num_t){{prefix}}_BUZZER_GPIO,
                   val.u32 != 0 ? {{prefix}}_BUZZER_ACTIVE_LEVEL : !{{prefix}}_BUZZER_ACTIVE_LEVEL);
#else
    if (val.u32 == 0) {
        /* silence: 0 duty */
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_BUZZER_CHANNEL, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_BUZZER_CHANNEL);
    } else {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t){{prefix}}_BUZZER_TIMER, val.u32);
        /* 50 % duty at the timer's resolution: a square wave is what a passive
         * piezo is loudest on. */
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_BUZZER_CHANNEL,
                      (1u << {{prefix}}_BUZZER_DUTY_BITS) / 2);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_BUZZER_CHANNEL);
    }
#endif
    return ESP_OK;
}
