case {{cfg.speed_param}}: {
    /* val.u8 is 0-100; LEDC 10-bit duty max = 1023 */
    uint8_t pct = val.u8 > 100 ? 100 : val.u8;
    uint32_t duty = ((uint32_t)pct * 1023U) / 100U;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_FAN_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_FAN_LEDC_CHANNEL);
    ESP_LOGI(TAG, "Fan {{prefix_lc}} speed: %u%%", (unsigned)pct);
    return ESP_OK;
}
