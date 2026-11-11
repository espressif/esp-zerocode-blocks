#ifndef ZC_ADC_UNIT_SHARED
#define ZC_ADC_UNIT_SHARED
/* ONE oneshot handle per ADC unit, shared by every block that reads one.
 * adc_oneshot_new_unit() CLAIMS the unit: a second call for the same unit
 * returns ESP_ERR_NOT_FOUND ("adcN is already in use"), and IDF keeps no
 * registry to look the first handle up again (unlike the I2C master driver,
 * which can be asked for a port's bus). So the first block to ask creates it and the
 * rest adopt it — two photodiodes, or one beside peripherals/adc_unit, used to
 * abort at boot. Guarded so the identical copy in each block's statics
 * compiles once; every copy must stay identical. */
static adc_oneshot_unit_handle_t s_zc_adc_units[SOC_ADC_PERIPH_NUM];
static esp_err_t zc_adc_unit_acquire(adc_unit_t unit, adc_oneshot_unit_handle_t *out)
{
    if ((unsigned)unit >= SOC_ADC_PERIPH_NUM) return ESP_ERR_INVALID_ARG;
    if (s_zc_adc_units[unit] == NULL) {
        adc_oneshot_unit_init_cfg_t cfg = {};
        cfg.unit_id = unit;
        esp_err_t err = adc_oneshot_new_unit(&cfg, &s_zc_adc_units[unit]);
        if (err != ESP_OK) {
            s_zc_adc_units[unit] = NULL;
            return err;
        }
    }
    *out = s_zc_adc_units[unit];
    return ESP_OK;
}
#endif

/* {{prefix_lc}}: analog input on GPIO{{cfg.gpio}} -> {{cfg.value_param}} */
#if {{prefix}}_AIN_SAMPLES < 1 || {{prefix}}_AIN_SAMPLES > 64
#error "analog_input {{prefix}}: samples must be 1-64"
#endif
#if {{prefix}}_AIN_POLL_MS < 20
#error "analog_input {{prefix}}: poll_ms must be at least 20"
#endif
#if {{prefix}}_AIN_DIV_NUM < 1 || {{prefix}}_AIN_DIV_DEN < 1
#error "analog_input {{prefix}}: divider_num and divider_den must be at least 1"
#endif
#if {{prefix}}_AIN_OUT_HI != {{prefix}}_AIN_OUT_LO && {{prefix}}_AIN_IN_HI == {{prefix}}_AIN_IN_LO
#error "analog_input {{prefix}}: in_hi_mv must differ from in_lo_mv when a map is set"
#endif
static adc_oneshot_unit_handle_t s_{{prefix_lc}}_adc = NULL;
static adc_channel_t s_{{prefix_lc}}_chan = (adc_channel_t)0;
static adc_cali_handle_t s_{{prefix_lc}}_cali = NULL;

/* Nominal full scale per attenuation, used only without a calibration scheme. */
static int {{prefix_lc}}_nominal_mv(int raw)
{
    static const int full[] = { 950, 1250, 1750, 3100 };
    int a = {{prefix}}_AIN_ATTEN;
    if (a < 0 || a > 3) a = 3;
    return (int)((int64_t)raw * full[a] / 4095);
}

/* Source mV -> the published value: the straight-line map, or mV itself. */
static int64_t {{prefix_lc}}_ain_scale(int64_t src_mv)
{
    if ({{prefix}}_AIN_OUT_HI == {{prefix}}_AIN_OUT_LO) return src_mv;
    int64_t num = (src_mv - {{prefix}}_AIN_IN_LO) * ((int64_t){{prefix}}_AIN_OUT_HI - {{prefix}}_AIN_OUT_LO);
    int64_t den = (int64_t){{prefix}}_AIN_IN_HI - {{prefix}}_AIN_IN_LO;
    int64_t q = (num >= 0) == (den >= 0) ? (llabs(num) + llabs(den) / 2) / llabs(den) : -((llabs(num) + llabs(den) / 2) / llabs(den));
    return {{prefix}}_AIN_OUT_LO + q;
}

static void {{prefix_lc}}_ain_poll(void *arg)
{
    (void)arg;
    if (s_{{prefix_lc}}_adc == NULL) return;
    int64_t sum = 0;
    int n = 0;
    for (int i = 0; i < {{prefix}}_AIN_SAMPLES; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_{{prefix_lc}}_adc, s_{{prefix_lc}}_chan, &raw) != ESP_OK) continue;
        int mv = 0;
        if (s_{{prefix_lc}}_cali == NULL || adc_cali_raw_to_voltage(s_{{prefix_lc}}_cali, raw, &mv) != ESP_OK) mv = {{prefix_lc}}_nominal_mv(raw);
        sum += mv;
        n++;
    }
{{#if cfg.status_param}}    {
        app_driver_param_val_t sv = {};
        sv.b = n > 0;
        app_driver_set_param({{cfg.status_param}}, sv, APP_DRIVER_SOURCE_LOCAL);
    }
{{/if}}    if (n == 0) return;
    int64_t pin_mv = (sum + n / 2) / n;
    int64_t src_mv = (pin_mv * {{prefix}}_AIN_DIV_NUM + {{prefix}}_AIN_DIV_DEN / 2) / {{prefix}}_AIN_DIV_DEN;
    int64_t v = {{prefix_lc}}_ain_scale(src_mv);
    app_driver_param_val_t out = {};
#if defined({{prefix}}_AIN_KIND_i16)
    out.i16 = (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
#elif defined({{prefix}}_AIN_KIND_u16)
    out.u16 = (uint16_t)(v < 0 ? 0 : v > 65535 ? 65535 : v);
#else
    out.u32 = (uint32_t)(v < 0 ? 0 : v > 4294967295LL ? 4294967295LL : v);
#endif
    app_driver_set_param({{cfg.value_param}}, out, APP_DRIVER_SOURCE_LOCAL);
}
