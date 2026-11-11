{
    /* listening: 1 = command window open (wake word heard), 0 = closed.
     * The fast blink is an OVERLAY on the shared pattern: when the window
     * closes, put back what was showing when it opened (in a voice+transport
     * product that is commissioning_led's state — unprovisioned, offline),
     * instead of a fixed "solid" that claimed the device was online. A state
     * change made while the window was open is left alone — unless it equals
     * the overlay's own pattern, which the close cannot tell apart from it. */
    static uint8_t s_{{prefix_lc}}_under = 1;
    static bool s_{{prefix_lc}}_open = false;
    app_driver_param_val_t {{prefix_lc}}_cur = {};
    if (listening) {
        if (!s_{{prefix_lc}}_open) {
            s_{{prefix_lc}}_under = app_driver_param_seen({{cfg.pattern_param}}) &&
                                    app_driver_get_param({{cfg.pattern_param}}, &{{prefix_lc}}_cur) == ESP_OK
                                        ? {{prefix_lc}}_cur.u8 : (uint8_t)1;
            s_{{prefix_lc}}_open = true;
        }
        app_driver_param_val_t v = { .u8 = 3 };
        app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    } else {
        s_{{prefix_lc}}_open = false;
        app_driver_get_param({{cfg.pattern_param}}, &{{prefix_lc}}_cur);
        if ({{prefix_lc}}_cur.u8 == 3) {
            app_driver_param_val_t v = { .u8 = s_{{prefix_lc}}_under };
            app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        }
    }
}
