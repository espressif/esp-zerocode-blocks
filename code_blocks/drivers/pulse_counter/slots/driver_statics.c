/* Written by the ISR, read by the publish timer under the same spinlock. */
static volatile uint32_t s_{{prefix_lc}}_pc_total = 0;
static volatile int64_t s_{{prefix_lc}}_pc_last_us = 0;
static portMUX_TYPE s_{{prefix_lc}}_pc_mux = portMUX_INITIALIZER_UNLOCKED;
/* Ring of (time, total) samples spanning the rate window. */
#define {{prefix}}_PC_SLOTS 32
static int64_t s_{{prefix_lc}}_pc_t[{{prefix}}_PC_SLOTS];
static uint32_t s_{{prefix_lc}}_pc_n[{{prefix}}_PC_SLOTS];
static int s_{{prefix_lc}}_pc_head = 0, s_{{prefix_lc}}_pc_fill = 0;

static void IRAM_ATTR {{prefix_lc}}_pc_isr(void *arg)
{
    (void)arg;
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&s_{{prefix_lc}}_pc_mux);
    if (now - s_{{prefix_lc}}_pc_last_us >= {{prefix}}_PC_MIN_US) {
        s_{{prefix_lc}}_pc_total = s_{{prefix_lc}}_pc_total + 1;
        s_{{prefix_lc}}_pc_last_us = now;
    }
    portEXIT_CRITICAL_ISR(&s_{{prefix_lc}}_pc_mux);
}

static void {{prefix_lc}}_pc_publish_cb(void *arg)
{
    (void)arg;
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_{{prefix_lc}}_pc_mux);
    uint32_t total = s_{{prefix_lc}}_pc_total;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_pc_mux);
    /* Record this sample, then find the oldest one still inside the window. */
    s_{{prefix_lc}}_pc_t[s_{{prefix_lc}}_pc_head] = now;
    s_{{prefix_lc}}_pc_n[s_{{prefix_lc}}_pc_head] = total;
    s_{{prefix_lc}}_pc_head = (s_{{prefix_lc}}_pc_head + 1) % {{prefix}}_PC_SLOTS;
    if (s_{{prefix_lc}}_pc_fill < {{prefix}}_PC_SLOTS) s_{{prefix_lc}}_pc_fill++;
    int64_t window_us = (int64_t){{prefix}}_PC_WINDOW_MS * 1000;
    int64_t t0 = now;
    uint32_t n0 = total;
    for (int k = 1; k < s_{{prefix_lc}}_pc_fill; k++) {
        int i = (s_{{prefix_lc}}_pc_head - 1 - k + 2 * {{prefix}}_PC_SLOTS) % {{prefix}}_PC_SLOTS;
        if (now - s_{{prefix_lc}}_pc_t[i] > window_us) break;
        t0 = s_{{prefix_lc}}_pc_t[i];
        n0 = s_{{prefix_lc}}_pc_n[i];
    }
    uint32_t rate = 0;
    if (now > t0) {
        uint64_t per_min = (uint64_t)(total - n0) * 60000000ULL / (uint64_t)(now - t0);
        rate = (uint32_t)(per_min / ({{prefix}}_PC_PER_UNIT > 0 ? {{prefix}}_PC_PER_UNIT : 1));
    }
    (void)rate;
{{#if cfg.count_param}}    {
        app_driver_param_val_t v = { .u32 = total };
        app_driver_set_param({{cfg.count_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    }
{{/if}}{{#if cfg.rate_param}}    {
        app_driver_param_val_t v = { .u32 = rate };
        app_driver_set_param({{cfg.rate_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    }
{{/if}}}
