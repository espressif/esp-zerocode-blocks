/* {{prefix_lc}}: vibration switch — pulses per minute over a window. */
static portMUX_TYPE s_{{prefix_lc}}_vib_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_{{prefix_lc}}_vib_count = 0;      /* guarded by the mux */
static int64_t s_{{prefix_lc}}_vib_last_us = 0;
static uint32_t s_{{prefix_lc}}_vib_stuck = 0;
static bool s_{{prefix_lc}}_vib_stuck_logged = false;
static uint64_t s_{{prefix_lc}}_vib_quiet_ms = 0;   /* since the last pulse */
static bool s_{{prefix_lc}}_vib_stale = false;

static void IRAM_ATTR {{prefix_lc}}_vib_isr(void *arg)
{
    (void)arg;
    int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&s_{{prefix_lc}}_vib_mux);
    if (now_us - s_{{prefix_lc}}_vib_last_us >= {{prefix}}_VIB_GAP_US) {
        s_{{prefix_lc}}_vib_last_us = now_us;
        s_{{prefix_lc}}_vib_count++;
    }
    portEXIT_CRITICAL_ISR(&s_{{prefix_lc}}_vib_mux);
}

static void {{prefix_lc}}_vib_window_cb(void *arg)
{
    (void)arg;
    portENTER_CRITICAL(&s_{{prefix_lc}}_vib_mux);
    uint32_t pulses = s_{{prefix_lc}}_vib_count;
    s_{{prefix_lc}}_vib_count = 0;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_vib_mux);

    if (pulses == 0 && gpio_get_level((gpio_num_t){{prefix}}_VIB_GPIO) == {{prefix}}_VIB_ACTIVE_LEVEL) {
        if (++s_{{prefix_lc}}_vib_stuck >= {{prefix}}_VIB_STUCK_WINDOWS && !s_{{prefix_lc}}_vib_stuck_logged) {
            s_{{prefix_lc}}_vib_stuck_logged = true;
            ESP_LOGW(TAG, "{{prefix_lc}}: GPIO %d held at its active level with no pulses — "
                     "active_level is inverted, or the module's sensitivity is turned up too far",
                     {{prefix}}_VIB_GPIO);
        }
    } else {
        s_{{prefix_lc}}_vib_stuck = 0;
        s_{{prefix_lc}}_vib_stuck_logged = false;
    }

    /* Sensor health: no pulse for stale_after_s, whatever any cycle logic
     * makes of the activity. Cleared by the next pulse. */
    if ({{prefix}}_VIB_STALE_MS > 0) {
        if (pulses > 0) {
            s_{{prefix_lc}}_vib_quiet_ms = 0;
            if (s_{{prefix_lc}}_vib_stale) {
                s_{{prefix_lc}}_vib_stale = false;
                ESP_LOGI(TAG, "{{prefix_lc}}: pulses again on GPIO %d — sensor alive", {{prefix}}_VIB_GPIO);
{{#if cfg.stale_param}}
                app_driver_param_val_t {{prefix_lc}}_ok = { .b = false };
                app_driver_set_param({{cfg.stale_param}}, {{prefix_lc}}_ok, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
            }
        } else if (!s_{{prefix_lc}}_vib_stale) {
            s_{{prefix_lc}}_vib_quiet_ms += {{prefix}}_VIB_WINDOW_MS;
            if (s_{{prefix_lc}}_vib_quiet_ms >= {{prefix}}_VIB_STALE_MS) {
                s_{{prefix_lc}}_vib_stale = true;
                ESP_LOGW(TAG, "{{prefix_lc}}: no pulse on GPIO %d for %llu s — sensor disconnected, or the machine has not run",
                         {{prefix}}_VIB_GPIO, (unsigned long long)({{prefix}}_VIB_STALE_MS / 1000ULL));
{{#if cfg.stale_param}}
                app_driver_param_val_t {{prefix_lc}}_st = { .b = true };
                app_driver_set_param({{cfg.stale_param}}, {{prefix_lc}}_st, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
            }
        }
    }

    uint64_t per_min = (uint64_t)pulses * 60000ULL / (uint64_t){{prefix}}_VIB_WINDOW_MS;
    app_driver_param_val_t v = { .u16 = (uint16_t)(per_min > 0xFFFF ? 0xFFFF : per_min) };
    app_driver_set_param({{cfg.activity_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
