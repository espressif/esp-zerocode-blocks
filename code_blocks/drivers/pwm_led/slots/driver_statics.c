/* {{prefix_lc}}: track power+brightness so we can reapply duty when either changes */
static bool s_{{prefix_lc}}_led_on = false;
static uint8_t s_{{prefix_lc}}_led_brightness = 254;
/* Full-scale duty at the resolution init chose for the frequency. */
static uint32_t s_{{prefix_lc}}_led_max_duty = 0;
static bool s_{{prefix_lc}}_led_ready = false;

static void {{prefix_lc}}_led_apply_duty(void)
{
    if (!s_{{prefix_lc}}_led_ready) {
        return;
    }
    uint32_t duty = 0;
    if (s_{{prefix_lc}}_led_on) {
        /* Map 0–254 → 0–max_duty. */
        duty = (uint32_t)(((uint64_t)s_{{prefix_lc}}_led_brightness * s_{{prefix_lc}}_led_max_duty) / 254U);
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_LED_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_LED_LEDC_CHANNEL);
}
