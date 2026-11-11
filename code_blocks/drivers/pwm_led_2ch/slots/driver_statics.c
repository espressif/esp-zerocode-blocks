static bool s_{{prefix_lc}}_on = false;
static uint8_t s_{{prefix_lc}}_brightness = 254;
static uint16_t s_{{prefix_lc}}_mireds = 250; /* mid-CCT */
/* Full-scale duty at the resolution init chose for the frequency. */
static uint32_t s_{{prefix_lc}}_max_duty = 0;
static bool s_{{prefix_lc}}_ready = false;

static void {{prefix_lc}}_apply(void)
{
    if (!s_{{prefix_lc}}_ready) {
        return;
    }
    uint32_t warm_duty = 0, cool_duty = 0;
    if (s_{{prefix_lc}}_on) {
        uint16_t mireds = s_{{prefix_lc}}_mireds;
        if (mireds < {{prefix}}_CT_MIN_MIREDS) mireds = {{prefix}}_CT_MIN_MIREDS;
        if (mireds > {{prefix}}_CT_MAX_MIREDS) mireds = {{prefix}}_CT_MAX_MIREDS;
        uint32_t span = ({{prefix}}_CT_MAX_MIREDS - {{prefix}}_CT_MIN_MIREDS);
        uint32_t warm_frac = ((uint32_t)(mireds - {{prefix}}_CT_MIN_MIREDS) * 255U) / span; /* 0..255 */
        uint32_t cool_frac = 255U - warm_frac;
        uint64_t scale = ((uint64_t)s_{{prefix_lc}}_brightness * s_{{prefix_lc}}_max_duty) / 254U; /* 0..max_duty */
        warm_duty = (uint32_t)((warm_frac * scale) / 255U);
        cool_duty = (uint32_t)((cool_frac * scale) / 255U);
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_WARM_CHANNEL, warm_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_WARM_CHANNEL);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_COOL_CHANNEL, cool_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_COOL_CHANNEL);
}
