case {{cfg.frequency_param}}: {
    if (val.u32 == 0) {
        /* silence: 0 duty */
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_BUZZER_CHANNEL, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_BUZZER_CHANNEL);
    } else {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t){{prefix}}_BUZZER_TIMER, val.u32);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_BUZZER_CHANNEL, 128); /* 50% duty */
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_BUZZER_CHANNEL);
    }
    return ESP_OK;
}
