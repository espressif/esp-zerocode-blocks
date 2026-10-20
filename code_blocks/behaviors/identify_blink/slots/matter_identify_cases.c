{
    /* The pattern param is shared: commissioning_led owns the underlying
     * state (2 unprovisioned, 1 online, 4 offline). Identify is an OVERLAY on
     * it, so STOP returns to what was showing at START, not to a fixed value —
     * a fixed "solid" after an Identify made an offline or unprovisioned device
     * claim to be online until the next connectivity event. If something
     * changed the pattern during identify (a real state change), that newer
     * value is left alone. idle_pattern is only the fallback when nothing had
     * set the pattern yet. */
    static uint8_t s_{{prefix_lc}}_under = {{prefix}}_IDLE_PATTERN;
    static bool s_{{prefix_lc}}_active = false;
    app_driver_param_val_t {{prefix_lc}}_cur = {};
    if (type == esp_matter::identification::callback_type_t::START) {
        if (!s_{{prefix_lc}}_active) {
            s_{{prefix_lc}}_under = app_driver_param_seen({{cfg.pattern_param}}) &&
                                    app_driver_get_param({{cfg.pattern_param}}, &{{prefix_lc}}_cur) == ESP_OK
                                        ? {{prefix_lc}}_cur.u8 : (uint8_t){{prefix}}_IDLE_PATTERN;
            s_{{prefix_lc}}_active = true;
        }
        app_driver_param_val_t v = { .u8 = {{prefix}}_IDENTIFY_PATTERN };
        app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    } else if (type == esp_matter::identification::callback_type_t::STOP) {
        s_{{prefix_lc}}_active = false;
        app_driver_get_param({{cfg.pattern_param}}, &{{prefix_lc}}_cur);
        if ({{prefix_lc}}_cur.u8 == {{prefix}}_IDENTIFY_PATTERN) {
            app_driver_param_val_t v = { .u8 = s_{{prefix_lc}}_under };
            app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        }
    }
}
