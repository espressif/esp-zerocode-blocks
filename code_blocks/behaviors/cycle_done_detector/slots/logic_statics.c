/* {{prefix_lc}}: appliance cycle running / done from an activity level. */
#if {{prefix}}_CYC_WINDOW_S < 10 || {{prefix}}_CYC_WINDOW_S > 600
#error "cycle_done_detector {{prefix}}: window_seconds must be 10-600"
#endif
#if {{prefix}}_CYC_START_S > {{prefix}}_CYC_WINDOW_S || {{prefix}}_CYC_BUSY_S > {{prefix}}_CYC_WINDOW_S
#error "cycle_done_detector {{prefix}}: active seconds cannot exceed window_seconds"
#endif

static uint8_t s_{{prefix_lc}}_cyc_ring[{{prefix}}_CYC_WINDOW_S];
static uint32_t s_{{prefix_lc}}_cyc_pos = 0;
static uint32_t s_{{prefix_lc}}_cyc_active = 0;    /* active seconds in the ring */
static int64_t s_{{prefix_lc}}_cyc_uptime_s = 0;   /* ticks since boot */
static uint32_t s_{{prefix_lc}}_cyc_idle_x64 = 0;  /* learned idle, x64 (64 s average) */
static bool s_{{prefix_lc}}_cyc_running = false;
static int64_t s_{{prefix_lc}}_cyc_start_s = 0;
static int64_t s_{{prefix_lc}}_cyc_busy_s = 0;     /* last tick the machine was busy */

static void {{prefix_lc}}_cyc_save(bool running)
{
    nvs_handle_t h;
    if (nvs_open({{prefix}}_CYC_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, {{prefix}}_CYC_NVS_KEY, running ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);
}

static void {{prefix_lc}}_cyc_publish(bool running, bool done)
{
    app_driver_param_val_t rv = { .b = running };
    app_driver_set_param({{cfg.running_param}}, rv, APP_DRIVER_SOURCE_LOCAL);
    app_driver_param_val_t dv = { .b = done };
    app_driver_set_param({{cfg.done_param}}, dv, APP_DRIVER_SOURCE_LOCAL);
}

static uint32_t {{prefix_lc}}_cyc_threshold(void)
{
    int32_t idle = {{prefix}}_CYC_IDLE_LEVEL >= 0 ? {{prefix}}_CYC_IDLE_LEVEL
                                                  : (int32_t)(s_{{prefix_lc}}_cyc_idle_x64 >> 6);
    int32_t thr = idle + {{prefix}}_CYC_MARGIN;
    return (uint32_t)(thr > {{prefix}}_CYC_MIN_LEVEL ? thr : {{prefix}}_CYC_MIN_LEVEL);
}

static void {{prefix_lc}}_cyc_tick_cb(void *arg)
{
    (void)arg;
    int64_t now_s = ++s_{{prefix_lc}}_cyc_uptime_s;
    app_driver_param_val_t av = {};
    uint32_t level = 0;
    if (app_driver_param_seen({{cfg.activity_param}}) &&
        app_driver_get_param({{cfg.activity_param}}, &av) == ESP_OK) {
        level = av.u16;
    }
    uint32_t thr = {{prefix_lc}}_cyc_threshold();
    uint8_t active = level >= thr ? 1 : 0;

    s_{{prefix_lc}}_cyc_active -= s_{{prefix_lc}}_cyc_ring[s_{{prefix_lc}}_cyc_pos];
    s_{{prefix_lc}}_cyc_ring[s_{{prefix_lc}}_cyc_pos] = active;
    s_{{prefix_lc}}_cyc_active += active;
    s_{{prefix_lc}}_cyc_pos = (s_{{prefix_lc}}_cyc_pos + 1) % {{prefix}}_CYC_WINDOW_S;

    if (!s_{{prefix_lc}}_cyc_running) {
        /* Learn idle only from a still window, never in the first one. */
        if ({{prefix}}_CYC_IDLE_LEVEL < 0 && now_s > {{prefix}}_CYC_WINDOW_S &&
            s_{{prefix_lc}}_cyc_active == 0) {
            uint32_t cap = 4U * {{prefix}}_CYC_MIN_LEVEL;
            uint32_t sample = level > cap ? cap : level;
            s_{{prefix_lc}}_cyc_idle_x64 += sample - (s_{{prefix_lc}}_cyc_idle_x64 >> 6);
        }
        if (s_{{prefix_lc}}_cyc_active >= {{prefix}}_CYC_START_S) {
            s_{{prefix_lc}}_cyc_running = true;
            s_{{prefix_lc}}_cyc_start_s = now_s;
            s_{{prefix_lc}}_cyc_busy_s = now_s;
            {{prefix_lc}}_cyc_save(true);
            {{prefix_lc}}_cyc_publish(true, false);
            ESP_LOGI(TAG, "{{prefix_lc}}: cycle started (activity %lu, threshold %lu)",
                     (unsigned long)level, (unsigned long)thr);
        }
        return;
    }

    if (s_{{prefix_lc}}_cyc_active >= {{prefix}}_CYC_BUSY_S) s_{{prefix_lc}}_cyc_busy_s = now_s;
    if (now_s - s_{{prefix_lc}}_cyc_busy_s < {{prefix}}_CYC_QUIET_S) return;

    /* Quiet long enough: the run is over. */
    int64_t run_s = s_{{prefix_lc}}_cyc_busy_s - s_{{prefix_lc}}_cyc_start_s;
    bool done = run_s >= {{prefix}}_CYC_MIN_RUN_S;
    s_{{prefix_lc}}_cyc_running = false;
    {{prefix_lc}}_cyc_save(false);
    {{prefix_lc}}_cyc_publish(false, done);
    if (done) {
        ESP_LOGI(TAG, "{{prefix_lc}}: cycle done (ran %lld min, then quiet %d min)",
                 (long long)(run_s / 60), {{prefix}}_CYC_QUIET_S / 60);
    } else {
        ESP_LOGI(TAG, "{{prefix_lc}}: activity for %lld s only — not a cycle, no done", (long long)run_s);
    }
}
