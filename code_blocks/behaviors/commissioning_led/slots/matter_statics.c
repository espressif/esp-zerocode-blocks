/* Paired or not comes from the framework's MATTER_COMMISSIONED — the fabric
 * table at boot, re-read on every fabric event — never from a connectivity
 * event: a paired device that reboots with the router down is still paired,
 * and one removed from one of several fabrics is still paired. Runs on the
 * Matter thread (every MATTER_COMMISSIONED write is made there). */
static app_driver_handle_t s_{{prefix_lc}}_matter_led = 0;

static bool {{prefix_lc}}_matter_paired(void)
{
    app_driver_param_val_t c = {};
    app_driver_get_param(APP_DRIVER_PARAM_MATTER_COMMISSIONED, &c);
    return app_driver_param_seen(APP_DRIVER_PARAM_MATTER_COMMISSIONED) && c.b;
}

static void {{prefix_lc}}_matter_led_cb(app_driver_param_id_t id, app_driver_param_val_t val,
                                        app_driver_handle_t source, void *ctx)
{
    (void)source; (void)ctx;
    app_driver_param_val_t cur = {};
    app_driver_get_param({{cfg.pattern_param}}, &cur);
    bool shown = app_driver_param_seen({{cfg.pattern_param}});
    app_driver_param_val_t v = {};
    if (id == APP_DRIVER_PARAM_MATTER_COMMISSIONED) {
        if (val.b) {
            /* Paired: leave a lost-network breathe (4) to the connectivity
             * events; anything showing "not set up" becomes online. */
            if (shown && cur.u8 != 2 && cur.u8 != 3) return;
            v.u8 = 1;
        } else {
            if (shown && cur.u8 == 3) return; /* a pairing in flight keeps its fast blink */
            v.u8 = 2;
        }
    } else if (id == {{cfg.pattern_param}} && val.u8 == 2 && {{prefix_lc}}_matter_paired()) {
        /* "Waiting to be set up" written on a paired device — logic_init's boot
         * default landing after the fabric read. */
        v.u8 = 1;
    } else {
        return;
    }
    app_driver_set_param({{cfg.pattern_param}}, v, s_{{prefix_lc}}_matter_led);
}

static void {{prefix_lc}}_matter_led_init(void)
{
    s_{{prefix_lc}}_matter_led = app_driver_register_solution("{{prefix_lc}}_matter_led", {{prefix_lc}}_matter_led_cb, NULL);
}
