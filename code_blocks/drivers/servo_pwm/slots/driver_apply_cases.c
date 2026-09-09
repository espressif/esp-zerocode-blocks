case {{cfg.position_param}}: {
    uint8_t deg = val.u8 > 180 ? 180 : val.u8;
    uint32_t duty = {{prefix}}_SERVO_DUTY_FOR_DEG(deg);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_SERVO_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_SERVO_CHANNEL);
    return ESP_OK;
}
