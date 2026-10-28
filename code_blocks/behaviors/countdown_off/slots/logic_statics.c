/* One 1 s tick while a countdown runs; the deadline is the truth, the tick
 * only publishes the minutes left and ends it. */
static esp_timer_handle_t s_{{prefix_lc}}_cd_tick = NULL;
static app_driver_handle_t s_{{prefix_lc}}_cd_handle = 0;
static portMUX_TYPE s_{{prefix_lc}}_cd_mux = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_{{prefix_lc}}_cd_deadline_us = 0;   /* 0 = none */
static int s_{{prefix_lc}}_cd_last_min = -1;

static void {{prefix_lc}}_cd_publish_remaining(int minutes)
{
{{#if cfg.remaining_param}}    if (minutes == s_{{prefix_lc}}_cd_last_min) return;
    app_driver_param_val_t v = { .u16 = (uint16_t)minutes };
    app_driver_set_param({{cfg.remaining_param}}, v, s_{{prefix_lc}}_cd_handle);
{{/if}}    s_{{prefix_lc}}_cd_last_min = minutes;
}

/* Stop the countdown; clear minutes_param when someone else did not already. */
static void {{prefix_lc}}_cd_stop(bool clear_minutes)
{
    portENTER_CRITICAL(&s_{{prefix_lc}}_cd_mux);
    s_{{prefix_lc}}_cd_deadline_us = 0;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_cd_mux);
    esp_timer_stop(s_{{prefix_lc}}_cd_tick);
    if (clear_minutes) {
        app_driver_param_val_t z = { .u16 = 0 };
        app_driver_set_param({{cfg.minutes_param}}, z, s_{{prefix_lc}}_cd_handle);
    }
    {{prefix_lc}}_cd_publish_remaining(0);
}

static void {{prefix_lc}}_cd_start(int minutes)
{
    if (minutes > {{prefix}}_CD_MAX_MIN) minutes = {{prefix}}_CD_MAX_MIN;
    portENTER_CRITICAL(&s_{{prefix_lc}}_cd_mux);
    s_{{prefix_lc}}_cd_deadline_us = esp_timer_get_time() + (int64_t)minutes * 60LL * 1000000LL;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_cd_mux);
    esp_timer_stop(s_{{prefix_lc}}_cd_tick);
    esp_timer_start_periodic(s_{{prefix_lc}}_cd_tick, 1000000ULL);
    {{prefix_lc}}_cd_publish_remaining(minutes);
    ESP_LOGI(TAG, "{{prefix_lc}}: off-timer %d min started", minutes);
#if {{prefix}}_CD_TURNS_ON
    app_driver_param_val_t on = { .b = true };
    app_driver_set_param({{cfg.target_param}}, on, s_{{prefix_lc}}_cd_handle);
#endif
}

static void {{prefix_lc}}_cd_tick_cb(void *arg)
{
    (void)arg;
    portENTER_CRITICAL(&s_{{prefix_lc}}_cd_mux);
    int64_t deadline = s_{{prefix_lc}}_cd_deadline_us;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_cd_mux);
    if (deadline == 0) return;
    int64_t left_us = deadline - esp_timer_get_time();
    if (left_us > 0) {
        {{prefix_lc}}_cd_publish_remaining((int)((left_us + 59999999LL) / 60000000LL));
        return;
    }
    ESP_LOGI(TAG, "{{prefix_lc}}: off-timer ended — switching {{cfg.target_param}} off");
    {{prefix_lc}}_cd_stop(true);
    app_driver_param_val_t off = { .b = false };
    app_driver_set_param({{cfg.target_param}}, off, s_{{prefix_lc}}_cd_handle);
}

static void {{prefix_lc}}_cd_cb(app_driver_param_id_t param_id, app_driver_param_val_t val,
                                 app_driver_handle_t source, void *ctx)
{
    (void)source; (void)ctx;
    if (s_{{prefix_lc}}_cd_tick == NULL) return;
    if (param_id == {{cfg.minutes_param}}) {
        if (val.u16 > 0) {{prefix_lc}}_cd_start(val.u16);
        else {{prefix_lc}}_cd_stop(false);
        return;
    }
#if {{prefix}}_CD_OFF_CANCELS
    if (param_id == {{cfg.target_param}} && !val.b) {
        portENTER_CRITICAL(&s_{{prefix_lc}}_cd_mux);
        bool running = s_{{prefix_lc}}_cd_deadline_us != 0;
        portEXIT_CRITICAL(&s_{{prefix_lc}}_cd_mux);
        if (running) {
            ESP_LOGI(TAG, "{{prefix_lc}}: {{cfg.target_param}} switched off — off-timer cancelled");
            {{prefix_lc}}_cd_stop(true);
        }
    }
#endif
}
