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
static led_strip_handle_t s_{{prefix_lc}}_strip = NULL;
static uint32_t s_{{prefix_lc}}_leds = 0;

static void {{prefix_lc}}_board_strip_set(bool on)
{
    if (s_{{prefix_lc}}_strip == NULL) {
        return;
    }
    if (!on) {
        led_strip_clear(s_{{prefix_lc}}_strip);
        return;
    }
    for (uint32_t i = 0; i < s_{{prefix_lc}}_leds; i++) {
        led_strip_set_pixel(s_{{prefix_lc}}_strip, i, {{cfg.red}}, {{cfg.green}}, {{cfg.blue}});
    }
    led_strip_refresh(s_{{prefix_lc}}_strip);
}

static void {{prefix_lc}}_board_strip_notify(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    (void)source; (void)ctx;
    if (param_id != {{cfg.param_id}}) {
        return;
    }
    ESP_LOGI(TAG, "Set {{prefix_lc}} ('{{cfg.device}}'): %s", val.b ? "ON" : "OFF");
    {{prefix_lc}}_board_strip_set(val.b);
}
