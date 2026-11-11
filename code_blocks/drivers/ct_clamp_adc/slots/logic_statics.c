extern adc_oneshot_unit_handle_t s_{{prefix_lc}}_adc;
extern adc_channel_t s_{{prefix_lc}}_adc_channel;
extern adc_cali_handle_t s_{{prefix_lc}}_cali;

#ifndef ZC_CT_CAL_SHARED
#define ZC_CT_CAL_SHARED
/* Zero reference, shared by every clamp in the product: ONE `ct-cal` console
 * command (registered by the first clamp), each clamp addressed by its
 * instance prefix when there is more than one. Guarded so the identical copy
 * in each instance's statics compiles once; every copy must stay identical. */
#define ZC_CT_CAL_NVS_NS     "zc_ct_cal"
#define ZC_CT_CAL_MAX_CLAMPS 4
#ifdef SOC_ADC_RTC_MAX_BITWIDTH
#define ZC_CT_ADC_BITS SOC_ADC_RTC_MAX_BITWIDTH
#else
#define ZC_CT_ADC_BITS 12
#endif

typedef struct {
    double mean_mv;   /* the bias this window */
    double rms_mv;    /* AC RMS around it, before the zero is removed */
    int samples;
} zc_ct_window_t;

typedef struct {
    const char *name;              /* the instance prefix, lower case */
    int32_t def_zero_uv;           /* build-time cfg, in uV */
    volatile int32_t zero_uv;      /* in use (an aligned word: the task reads it whole) */
    bool zero_set;                 /* the owner recorded it */
    bool (*sample)(zc_ct_window_t *out);
    void (*print)(const zc_ct_window_t *w);
} zc_ct_clamp_t;

static zc_ct_clamp_t *s_zc_ct_clamps[ZC_CT_CAL_MAX_CLAMPS];
static int s_zc_ct_clamp_count = 0;

/* Nominal full-scale mV per attenuation, for a chip with no calibration scheme. */
static int zc_ct_raw_to_mv(adc_cali_handle_t cali, int atten, int raw)
{
    int mv = 0;
    if (cali && adc_cali_raw_to_voltage(cali, raw, &mv) == ESP_OK) return mv;
    static const int full[4] = { 950, 1250, 1750, 3100 };
    int fs = full[(atten < 0 || atten > 3) ? 3 : atten];
    return (int)(((int64_t)raw * fs) / ((1 << ZC_CT_ADC_BITS) - 1));
}

/* Signal RMS with the zero removed in quadrature, never negative. */
static double zc_ct_signal_mv(double rms_mv, int32_t zero_uv)
{
    double z = zero_uv / 1000.0;
    double s2 = rms_mv * rms_mv - z * z;
    return s2 > 0 ? sqrt(s2) : 0.0;
}

static void zc_ct_key(const zc_ct_clamp_t *c, char *key, size_t n)
{
    snprintf(key, n, "%.15s", c->name);  /* NVS keys are at most 15 characters */
}

static void zc_ct_load(zc_ct_clamp_t *c)
{
    c->zero_uv = c->def_zero_uv;
    c->zero_set = false;
    nvs_handle_t h;
    if (nvs_open(ZC_CT_CAL_NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    char key[16];
    zc_ct_key(c, key, sizeof(key));
    int32_t v = 0;
    if (nvs_get_i32(h, key, &v) == ESP_OK && v >= 0) {
        c->zero_uv = v;
        c->zero_set = true;
    }
    nvs_close(h);
}

static esp_err_t zc_ct_store(const zc_ct_clamp_t *c)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(ZC_CT_CAL_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    char key[16];
    zc_ct_key(c, key, sizeof(key));
    if (!c->zero_set) {
        err = nvs_erase_key(h, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    } else {
        err = nvs_set_i32(h, key, c->zero_uv);
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static void zc_ct_usage(void)
{
    printf("usage: ct-cal show|zero|reset%s\n", s_zc_ct_clamp_count > 1 ? " [clamp]" : "");
    printf("  zero   nothing switched on through the clamp: store its idle noise\n");
    printf("  show   noise now, the stored zero, current and power\n");
    printf("  reset  back to the firmware's built-in zero\n");
    if (s_zc_ct_clamp_count > 1) {
        printf("clamps:");
        for (int i = 0; i < s_zc_ct_clamp_count; i++) printf(" %s", s_zc_ct_clamps[i]->name);
        printf("\n");
    }
}

static int zc_ct_cmd(int argc, char **argv)
{
    const char *verb = argc >= 2 ? argv[1] : "show";
    const char *which = argc >= 3 ? argv[2] : NULL;
    bool show = strcmp(verb, "show") == 0;
    bool zero = strcmp(verb, "zero") == 0;
    bool reset = strcmp(verb, "reset") == 0;
    if (!(show || zero || reset) || argc > 3) {
        zc_ct_usage();
        return 1;
    }
    zc_ct_clamp_t *c = NULL;
    if (which != NULL) {
        for (int i = 0; i < s_zc_ct_clamp_count; i++) {
            if (strcasecmp(which, s_zc_ct_clamps[i]->name) == 0) c = s_zc_ct_clamps[i];
        }
        if (c == NULL) {
            printf("no clamp named \"%s\"\n", which);
            zc_ct_usage();
            return 1;
        }
    } else if (s_zc_ct_clamp_count == 1) {
        c = s_zc_ct_clamps[0];
    } else {
        printf("this device has %d clamps; name one: ct-cal %s <clamp>\n", s_zc_ct_clamp_count, verb);
        zc_ct_usage();
        return 1;
    }

    zc_ct_window_t w = {};
    if (reset) {
        c->zero_uv = c->def_zero_uv;
        c->zero_set = false;
    } else if (zero) {
        if (!c->sample(&w)) {
            printf("%s: no reading (the clamp's pin has no working ADC channel) - nothing saved\n", c->name);
            return 1;
        }
        c->zero_uv = (int32_t)(w.rms_mv * 1000.0);
        c->zero_set = true;
    }
    if (zero || reset) {
        esp_err_t err = zc_ct_store(c);
        if (err != ESP_OK) {
            printf("%s: could not save to flash (%s) - in use until the next reboot\n", c->name, esp_err_to_name(err));
        }
    }
    if (!c->sample(&w)) {
        printf("%s: no reading (the clamp's pin has no working ADC channel)\n", c->name);
        return 1;
    }
    c->print(&w);
    return 0;
}

static void zc_ct_add(zc_ct_clamp_t *c)
{
    zc_ct_load(c);
    if (c->zero_set) {
        ESP_LOGI(TAG, "current clamp %s: zero from flash (%.1f mV)", c->name, c->zero_uv / 1000.0);
    }
    if (s_zc_ct_clamp_count >= ZC_CT_CAL_MAX_CLAMPS) {
        ESP_LOGW(TAG, "current clamp %s: more than %d clamps - not on ct-cal", c->name, ZC_CT_CAL_MAX_CLAMPS);
        return;
    }
    s_zc_ct_clamps[s_zc_ct_clamp_count++] = c;
    if (s_zc_ct_clamp_count == 1) {
        /* Zero-init + assign: newer IDF adds fields to esp_console_cmd_t. */
        esp_console_cmd_t cmd = {};
        cmd.command = "ct-cal";
        cmd.help = "Current clamp zero: ct-cal zero (nothing switched on) | show | reset";
        cmd.func = &zc_ct_cmd;
        esp_console_cmd_register(&cmd);
    }
}
#endif

/* One window: 10 whole mains cycles, sampled as fast as the ADC goes. The
 * console and the task may both sample; the mutex keeps them apart. */
static SemaphoreHandle_t s_{{prefix_lc}}_ct_lock = NULL;

static bool {{prefix_lc}}_ct_sample(zc_ct_window_t *out)
{
    if (s_{{prefix_lc}}_adc == NULL || s_{{prefix_lc}}_ct_lock == NULL) return false;
    xSemaphoreTake(s_{{prefix_lc}}_ct_lock, portMAX_DELAY);
    const int64_t window_us = 10LL * 1000000LL / ({{prefix}}_CT_MAINS_HZ > 0 ? {{prefix}}_CT_MAINS_HZ : 50);
    int64_t sum = 0, sum_sq = 0;
    int n = 0;
    const int64_t t0 = esp_timer_get_time();
    while (esp_timer_get_time() - t0 < window_us) {
        int raw = 0;
        if (adc_oneshot_read(s_{{prefix_lc}}_adc, s_{{prefix_lc}}_adc_channel, &raw) != ESP_OK) continue;
        int mv = zc_ct_raw_to_mv(s_{{prefix_lc}}_cali, {{prefix}}_CT_ATTEN, raw);
        sum += mv;
        sum_sq += (int64_t)mv * mv;
        n++;
    }
    xSemaphoreGive(s_{{prefix_lc}}_ct_lock);
    if (n < 20) return false;
    double mean = (double)sum / n;
    double var = (double)sum_sq / n - mean * mean;
    out->mean_mv = mean;
    out->rms_mv = var > 0 ? sqrt(var) : 0.0;
    out->samples = n;
    return true;
}

static void {{prefix_lc}}_ct_print(const zc_ct_window_t *w);

static zc_ct_clamp_t s_{{prefix_lc}}_ct = {
    "{{prefix_lc}}", {{prefix}}_CT_ZERO_MV * 1000, {{prefix}}_CT_ZERO_MV * 1000, false,
    &{{prefix_lc}}_ct_sample, &{{prefix_lc}}_ct_print,
};

static uint32_t {{prefix_lc}}_ct_current_ma(const zc_ct_window_t *w)
{
    double ma = zc_ct_signal_mv(w->rms_mv, s_{{prefix_lc}}_ct.zero_uv) * {{prefix}}_CT_A_PER_V;  /* mV x A/V = mA */
    if (ma < {{prefix}}_CT_MIN_MA) return 0;
    return (uint32_t)(ma + 0.5);
}

static void {{prefix_lc}}_ct_print(const zc_ct_window_t *w)
{
    uint32_t ma = {{prefix_lc}}_ct_current_ma(w);
    printf("%s: bias %.0f mV, noise/signal %.1f mV RMS (%d samples), zero %.1f mV (%s) -> %lu mA, %lu W\n",
           s_{{prefix_lc}}_ct.name, w->mean_mv, w->rms_mv, w->samples, s_{{prefix_lc}}_ct.zero_uv / 1000.0,
           s_{{prefix_lc}}_ct.zero_set ? "recorded" : "built-in", (unsigned long)ma,
           (unsigned long)(((uint64_t)ma * {{prefix}}_CT_MAINS_V * {{prefix}}_CT_PF_PCT / 100) / 1000));
}

static void {{prefix_lc}}_ct_task(void *arg)
{
    (void)arg;
    for (;;) {
        zc_ct_window_t w = {};
        if ({{prefix_lc}}_ct_sample(&w)) {
            uint32_t ma = {{prefix_lc}}_ct_current_ma(&w);
            app_driver_param_val_t v = { .u32 = ma };
            app_driver_set_param({{cfg.current_param}}, v, APP_DRIVER_SOURCE_LOCAL);
{{#if cfg.power_param}}
            app_driver_param_val_t p = { .u32 = (uint32_t)((uint64_t)ma * {{prefix}}_CT_MAINS_V * {{prefix}}_CT_PF_PCT / 100) };
            app_driver_set_param({{cfg.power_param}}, p, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
            ESP_LOGD(TAG, "current clamp {{prefix_lc}}: %.1f mV RMS -> %lu mA", w.rms_mv, (unsigned long)ma);
        }
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_CT_POLL_MS > 300 ? {{prefix}}_CT_POLL_MS - 200 : 100));
    }
}
