/* {{prefix_lc}}: the board's own '{{cfg.device}}' led_strip device.
 * The pin, LED count and RMT/SPI backend are read from the BOARD at init — no
 * GPIO number appears anywhere in this product, which is the point: the line
 * belongs to the board definition and claiming it here would be an IO
 * conflict with it. NULL until init resolves the handle, and every write is a
 * no-op until then, so a missing device degrades to "does nothing".
 *
 * An OBSERVER, not an owner: this block registers a notify callback instead
 * of contributing an apply case, so it can indicate a param another driver
 * owns (a relay's POWER) without emitting a duplicate `case` label in
 * app_driver_apply_param(). */
{{#if cfg.hue_param}}#define {{prefix}}_COLOUR_MODE 1
{{/if}}static led_strip_handle_t s_{{prefix_lc}}_strip = NULL;
static uint32_t s_{{prefix_lc}}_leds = 0;
static bool s_{{prefix_lc}}_on = false;
static uint8_t s_{{prefix_lc}}_bri = 254;

/* Hue, saturation and brightness are on Matter's 0..254 scales, but the bus
 * carries a plain u8: anything else that sets them (a console command, an MQTT
 * or AWS payload) can send 255. Clamped on the way in — at 255 the brightness
 * scaling below computes 256, which wraps to 0 and turns the LED OFF, and a
 * hue of 255 falls out of the six colour sectors. */
static inline uint8_t {{prefix_lc}}_scale254(uint8_t v) { return v > 254 ? 254 : v; }

#ifdef {{prefix}}_COLOUR_MODE
static uint8_t s_{{prefix_lc}}_hue = 0;
static uint8_t s_{{prefix_lc}}_sat = 254;

/* HSV on Matter's 0..254 scales → 0..255 RGB (six 42.5-step hue sectors). */
static void {{prefix_lc}}_hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) { *r = *g = *b = v; return; }
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
#endif

static void {{prefix_lc}}_board_strip_render(void)
{
    if (s_{{prefix_lc}}_strip == NULL) {
        return;
    }
    if (!s_{{prefix_lc}}_on) {
        led_strip_clear(s_{{prefix_lc}}_strip);
        return;
    }
    uint8_t r, g, b;
#ifdef {{prefix}}_COLOUR_MODE
    {{prefix_lc}}_hsv_to_rgb(s_{{prefix_lc}}_hue, s_{{prefix_lc}}_sat, (uint8_t)((uint16_t)s_{{prefix_lc}}_bri * 255U / 254U), &r, &g, &b);
#else
    r = (uint8_t)((uint16_t){{cfg.red}}   * s_{{prefix_lc}}_bri / 254U);
    g = (uint8_t)((uint16_t){{cfg.green}} * s_{{prefix_lc}}_bri / 254U);
    b = (uint8_t)((uint16_t){{cfg.blue}}  * s_{{prefix_lc}}_bri / 254U);
#endif
    for (uint32_t i = 0; i < s_{{prefix_lc}}_leds; i++) {
        led_strip_set_pixel(s_{{prefix_lc}}_strip, i, r, g, b);
    }
    led_strip_refresh(s_{{prefix_lc}}_strip);
}

/* Kept for the shape every other indicator uses: on/off straight through. */
static void {{prefix_lc}}_board_strip_set(bool on)
{
    s_{{prefix_lc}}_on = on;
    {{prefix_lc}}_board_strip_render();
}

static void {{prefix_lc}}_board_strip_notify(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    (void)source; (void)ctx;
    if (param_id == {{cfg.param_id}}) {
        ESP_LOGI(TAG, "Set {{prefix_lc}} ('{{cfg.device}}'): %s", val.b ? "ON" : "OFF");
        {{prefix_lc}}_board_strip_set(val.b);
        return;
    }
{{#if cfg.hue_param}}    if (param_id == {{cfg.hue_param}}) { s_{{prefix_lc}}_hue = {{prefix_lc}}_scale254(val.u8); {{prefix_lc}}_board_strip_render(); return; }
{{/if}}{{#if cfg.saturation_param}}#ifdef {{prefix}}_COLOUR_MODE   /* saturation means nothing without a hue */
    if (param_id == {{cfg.saturation_param}}) { s_{{prefix_lc}}_sat = {{prefix_lc}}_scale254(val.u8); {{prefix_lc}}_board_strip_render(); return; }
#endif
{{/if}}{{#if cfg.brightness_param}}    if (param_id == {{cfg.brightness_param}}) { s_{{prefix_lc}}_bri = {{prefix_lc}}_scale254(val.u8); {{prefix_lc}}_board_strip_render(); return; }
{{/if}}}
