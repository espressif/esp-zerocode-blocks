static bool s_{{prefix_lc}}_on = false;
static uint8_t s_{{prefix_lc}}_brightness = 254;
static uint8_t s_{{prefix_lc}}_hue = 0;
static uint8_t s_{{prefix_lc}}_sat = 254;

/* HSB→RGB, hue/sat 0-254 (Matter encoding), value 0-254 → out 0-255 each */
static void {{prefix_lc}}_hsb_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) { *r = *g = *b = v; return; }
    /* Matter hue is 0..254 over six 42.5-step sectors. remainder is the
       position inside the sector, scaled to 0..255. */
    uint16_t h6 = (uint16_t)h * 6U;
    uint16_t region = h6 / 255U;
    uint16_t remainder = (h6 % 255U) * 256U / 255U;
    uint8_t p = ((uint16_t)v * (255U - s)) / 255U;
    uint8_t q = ((uint16_t)v * (255U - (uint16_t)s * remainder / 255U)) / 255U;
    uint8_t t = ((uint16_t)v * (255U - (uint16_t)s * (255U - remainder) / 255U)) / 255U;
    switch (region) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

static void {{prefix_lc}}_apply(void)
{
    uint8_t r = 0, g = 0, b = 0;
    if (s_{{prefix_lc}}_on) {
        {{prefix_lc}}_hsb_to_rgb(s_{{prefix_lc}}_hue, s_{{prefix_lc}}_sat, s_{{prefix_lc}}_brightness, &r, &g, &b);
    }
    uint32_t rd = ((uint32_t)r * 8191U) / 255U;
    uint32_t gd = ((uint32_t)g * 8191U) / 255U;
    uint32_t bd = ((uint32_t)b * 8191U) / 255U;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_R_CH, rd);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_R_CH);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_G_CH, gd);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_G_CH);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_B_CH, bd);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_B_CH);
}
