/* {{prefix_lc}}: {{cfg.mode}} of {{cfg.source_param}} over {{cfg.samples}} samples -> {{cfg.output_param}} */
#if {{prefix}}_FLT_SAMPLES < 1 || {{prefix}}_FLT_SAMPLES > 32
#error "sensor_filter {{prefix}}: samples must be 1-32"
#endif
#if {{prefix}}_FLT_PERIOD_MS < 10
#error "sensor_filter {{prefix}}: sample_ms must be at least 10"
#endif
static_assert({{cfg.source_param}} != {{cfg.output_param}}, "sensor_filter {{prefix}}: output_param must differ from source_param");

#if defined({{prefix}}_FLT_KIND_i16)
#define {{prefix}}_FLT_MIN (-32768LL)
#define {{prefix}}_FLT_MAX (32767LL)
#elif defined({{prefix}}_FLT_KIND_u16)
#define {{prefix}}_FLT_MIN (0LL)
#define {{prefix}}_FLT_MAX (65535LL)
#else
#define {{prefix}}_FLT_MIN (0LL)
#define {{prefix}}_FLT_MAX (4294967295LL)
#endif

/* Touched only on the poll task. */
static int64_t s_{{prefix_lc}}_flt_ring[{{prefix}}_FLT_SAMPLES];
static int s_{{prefix_lc}}_flt_pos = 0;
static int s_{{prefix_lc}}_flt_count = 0;

/* Nearest integer of sum / n, halves away from zero. */
static int64_t {{prefix_lc}}_flt_div_round(int64_t sum, int64_t n)
{
    return sum >= 0 ? (sum + n / 2) / n : -((-sum + n / 2) / n);
}

static int64_t {{prefix_lc}}_flt_value(void)
{
    int n = s_{{prefix_lc}}_flt_count;
#if defined({{prefix}}_FLT_MODE_median)
    int64_t v[{{prefix}}_FLT_SAMPLES];
    for (int i = 0; i < n; i++) v[i] = s_{{prefix_lc}}_flt_ring[i];
    for (int i = 1; i < n; i++) {
        int64_t x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
    return (n & 1) ? v[n / 2] : {{prefix_lc}}_flt_div_round(v[n / 2 - 1] + v[n / 2], 2);
#else
    int64_t sum = 0;
    for (int i = 0; i < n; i++) sum += s_{{prefix_lc}}_flt_ring[i];
    return {{prefix_lc}}_flt_div_round(sum, n);
#endif
}

static void {{prefix_lc}}_flt_sample(void *arg)
{
    (void)arg;
    if (!app_driver_param_seen({{cfg.source_param}})) return;
    if ({{prefix}}_FLT_STALE_MS > 0 &&
        esp_timer_get_time() - app_driver_param_written_us({{cfg.source_param}}) > (int64_t){{prefix}}_FLT_STALE_MS * 1000) {
        return;   /* the sensor stopped writing: nothing new to add */
    }
    app_driver_param_val_t in = {};
    if (app_driver_get_param({{cfg.source_param}}, &in) != ESP_OK) return;
    s_{{prefix_lc}}_flt_ring[s_{{prefix_lc}}_flt_pos] = (int64_t)in.{{cfg.value_kind}};
    s_{{prefix_lc}}_flt_pos = (s_{{prefix_lc}}_flt_pos + 1) % {{prefix}}_FLT_SAMPLES;
    if (s_{{prefix_lc}}_flt_count < {{prefix}}_FLT_SAMPLES) s_{{prefix_lc}}_flt_count++;
    int64_t f = {{prefix_lc}}_flt_value();
    if (f < {{prefix}}_FLT_MIN) f = {{prefix}}_FLT_MIN;
    if (f > {{prefix}}_FLT_MAX) f = {{prefix}}_FLT_MAX;
    app_driver_param_val_t out = {};
#if defined({{prefix}}_FLT_KIND_i16)
    out.i16 = (int16_t)f;
#elif defined({{prefix}}_FLT_KIND_u16)
    out.u16 = (uint16_t)f;
#else
    out.u32 = (uint32_t)f;
#endif
    app_driver_set_param({{cfg.output_param}}, out, APP_DRIVER_SOURCE_LOCAL);
}
