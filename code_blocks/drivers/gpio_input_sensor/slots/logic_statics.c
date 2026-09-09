/* Debounce = N consecutive polls agreeing, N = ceil(debounce_ms / poll_ms), at least 1. */
#define {{prefix}}_INPUT_DEBOUNCE_POLLS \
    (({{prefix}}_INPUT_DEBOUNCE_MS <= 0) ? 1 : \
     (({{prefix}}_INPUT_DEBOUNCE_MS + {{prefix}}_INPUT_POLL_MS - 1) / {{prefix}}_INPUT_POLL_MS))

static bool s_{{prefix_lc}}_state = false;      /* the debounced, published level */
static bool s_{{prefix_lc}}_candidate = false;  /* the level being confirmed */
static int  s_{{prefix_lc}}_agree = 0;          /* consecutive polls at the candidate */
static bool s_{{prefix_lc}}_primed = false;

static void {{prefix_lc}}_publish(bool asserted)
{
    app_driver_param_val_t pv = { .b = asserted };
    app_driver_set_param({{cfg.target_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
}

static void {{prefix_lc}}_input_poll_cb(void *arg)
{
    int level = gpio_get_level((gpio_num_t){{prefix}}_INPUT_GPIO);
    bool asserted = (level == {{prefix}}_INPUT_ACTIVE_LEVEL);

    if (!s_{{prefix_lc}}_primed) {
        /* First poll seeds the state from the real level, no debounce: the
         * product must know where its input stands the moment it boots. */
        s_{{prefix_lc}}_primed = true;
        s_{{prefix_lc}}_state = s_{{prefix_lc}}_candidate = asserted;
        s_{{prefix_lc}}_agree = {{prefix}}_INPUT_DEBOUNCE_POLLS;
        {{prefix_lc}}_publish(asserted);
        ESP_LOGI(TAG, "{{prefix_lc}}: %s (boot)", asserted ? "ASSERTED" : "cleared");
        return;
    }

    if (asserted != s_{{prefix_lc}}_candidate) {
        s_{{prefix_lc}}_candidate = asserted;
        s_{{prefix_lc}}_agree = 1;
    } else if (s_{{prefix_lc}}_agree < {{prefix}}_INPUT_DEBOUNCE_POLLS) {
        s_{{prefix_lc}}_agree++;
    }

    if (s_{{prefix_lc}}_agree >= {{prefix}}_INPUT_DEBOUNCE_POLLS && s_{{prefix_lc}}_candidate != s_{{prefix_lc}}_state) {
        s_{{prefix_lc}}_state = s_{{prefix_lc}}_candidate;
        {{prefix_lc}}_publish(s_{{prefix_lc}}_state);
        ESP_LOGI(TAG, "{{prefix_lc}}: %s", s_{{prefix_lc}}_state ? "ASSERTED" : "cleared");
        return;
    }

    /* Re-assert: this driver owns the param. If something else wrote it (a
     * bench console command, a controller keeping its own shadow), the bus
     * now disagrees with the hardware — put the real reading back. */
    app_driver_param_val_t cur = {};
    if (app_driver_get_param({{cfg.target_param}}, &cur) == ESP_OK && cur.b != s_{{prefix_lc}}_state) {
        ESP_LOGW(TAG, "{{prefix_lc}}: bus said %s, input is %s — re-asserting",
                 cur.b ? "ASSERTED" : "cleared", s_{{prefix_lc}}_state ? "ASSERTED" : "cleared");
        {{prefix_lc}}_publish(s_{{prefix_lc}}_state);
    }
}
