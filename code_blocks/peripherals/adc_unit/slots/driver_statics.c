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

static adc_oneshot_unit_handle_t s_{{prefix_lc}}_adc = NULL;
adc_oneshot_unit_handle_t {{prefix_lc}}_adc_handle(void) { return s_{{prefix_lc}}_adc; }
