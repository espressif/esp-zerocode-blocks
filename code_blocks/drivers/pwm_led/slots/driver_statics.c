/* {{prefix_lc}}: track power+brightness so we can reapply duty when either changes */
static bool s_{{prefix_lc}}_led_on = false;
static uint8_t s_{{prefix_lc}}_led_brightness = 254;

static void {{prefix_lc}}_led_apply_duty(void)
{
    uint32_t duty = 0;
    if (s_{{prefix_lc}}_led_on) {
        /* LEDC 13-bit timer: max duty = 8191. Map 0–254 → 0–8191. */
        duty = ((uint32_t)s_{{prefix_lc}}_led_brightness * 8191U) / 254U;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_LED_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_LED_LEDC_CHANNEL);
}
