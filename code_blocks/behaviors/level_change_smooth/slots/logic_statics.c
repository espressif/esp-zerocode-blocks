static volatile uint8_t s_{{prefix_lc}}_target = 0;
static volatile uint8_t s_{{prefix_lc}}_current = 0;
static volatile bool s_{{prefix_lc}}_internal_set = false;

static void {{prefix_lc}}_smooth_cb(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    if (param_id != {{cfg.target_param}}) return;
    /* Skip our own propagated steps to avoid feedback */
    if (s_{{prefix_lc}}_internal_set) return;
    s_{{prefix_lc}}_target = val.u8;
}

static void {{prefix_lc}}_smooth_task(void *arg)
{
    for (;;) {
        uint8_t t = s_{{prefix_lc}}_target;
        uint8_t c = s_{{prefix_lc}}_current;
        if (t != c) {
            int diff = (int)t - (int)c;
            int step = {{prefix}}_SMOOTH_STEP;
            if (diff >  step) c += step;
            else if (diff < -step) c -= step;
            else c = t;
            s_{{prefix_lc}}_current = c;
            s_{{prefix_lc}}_internal_set = true;
            app_driver_param_val_t pv = { .u8 = c };
            app_driver_set_param({{cfg.target_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
            s_{{prefix_lc}}_internal_set = false;
        }
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_SMOOTH_TICK_MS));
    }
}
