/* The requested level, handed from the bus callback (any task) to the ramp
 * task. A spinlock rather than a volatile pair, so the task never reads a new
 * level with a stale "have one" flag. */
static portMUX_TYPE s_{{prefix_lc}}_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_{{prefix_lc}}_target = 0;
static bool s_{{prefix_lc}}_have_target = false;

static void {{prefix_lc}}_smooth_cb(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    /* Only the REQUESTED level. The ramp's own writes go to output_param, so
       there is no feedback to filter and no real request can be mistaken for
       one of our steps. */
    if (param_id != {{cfg.target_param}}) return;
    portENTER_CRITICAL(&s_{{prefix_lc}}_lock);
    s_{{prefix_lc}}_target = val.u8;
    s_{{prefix_lc}}_have_target = true;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_lock);
}

static void {{prefix_lc}}_smooth_task(void *arg)
{
    for (;;) {
        portENTER_CRITICAL(&s_{{prefix_lc}}_lock);
        bool have = s_{{prefix_lc}}_have_target;
        uint8_t t = s_{{prefix_lc}}_target;
        portEXIT_CRITICAL(&s_{{prefix_lc}}_lock);
        if (have) {
            /* Step from what the driver actually holds, so a restored or
               externally-set level is the ramp's start, not 0. Before the
               driver has applied anything there is no level to ramp from:
               write the request straight through. */
            uint8_t c = t;
            bool move = true;
            if (app_driver_param_seen({{cfg.output_param}})) {
                app_driver_param_val_t cur = {};
                app_driver_get_param({{cfg.output_param}}, &cur);
                c = cur.u8;
                move = (c != t);
                int diff = (int)t - (int)c;
                int step = {{prefix}}_SMOOTH_STEP;
                if (diff >  step) c += step;
                else if (diff < -step) c -= step;
                else c = t;
            }
            if (move) {
                app_driver_param_val_t pv = { .u8 = c };
                app_driver_set_param({{cfg.output_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
            }
        }
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_SMOOTH_TICK_MS));
    }
}
