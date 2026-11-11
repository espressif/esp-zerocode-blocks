case {{cfg.speed_param}}: {
    /* val.u8 is 0-100, mapped onto the duty range init chose. */
    uint8_t pct = val.u8 > 100 ? 100 : val.u8;
    if (s_{{prefix_lc}}_fan_max_duty == 0) {
        return ESP_OK; /* init could not reach the frequency, and said so */
    }
    uint32_t duty = (uint32_t)(((uint64_t)pct * s_{{prefix_lc}}_fan_max_duty) / 100U);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_FAN_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_FAN_LEDC_CHANNEL);
    ESP_LOGI(TAG, "Fan {{prefix_lc}} speed: %u%%", (unsigned)pct);
    return ESP_OK;
}
