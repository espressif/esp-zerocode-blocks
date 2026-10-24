extern adc_oneshot_unit_handle_t s_{{prefix_lc}}_adc;
extern adc_channel_t s_{{prefix_lc}}_adc_channel;

#ifndef ZC_SOIL_CAL_SHARED
#define ZC_SOIL_CAL_SHARED
/* Runtime calibration, shared by every soil probe in the product: ONE
 * `soil-cal` console command (registered by the first probe), each probe
 * addressed by its instance prefix when there is more than one. The owner's
 * dry/wet points live in NVS (namespace zc_soil_cal, one blob per probe keyed
 * by the prefix) and are loaded over the build-time cfg at boot, so a factory
 * reset (nvs_flash_erase) returns every probe to its cfg defaults. Guarded so
 * the identical copy in each instance's statics compiles once; every copy must
 * stay identical. */
#define ZC_SOIL_CAL_NVS_NS     "zc_soil_cal"
#define ZC_SOIL_CAL_MIN_SPAN   100   /* raw counts; closer than this is not a calibration */
#define ZC_SOIL_CAL_SAMPLES    32    /* a calibration reading averages more than a poll */
#define ZC_SOIL_CAL_MAX_PROBES 4
#define ZC_SOIL_CAL_DRY_SET    0x1
#define ZC_SOIL_CAL_WET_SET    0x2

typedef struct {
    const char *name;            /* the instance prefix, lower case */
    int32_t def_dry, def_wet;    /* build-time cfg */
    int32_t dry, wet;            /* in use (an aligned word: the poll reads it whole) */
    uint32_t set;                /* ZC_SOIL_CAL_*_SET: which points the owner set */
    bool (*read_raw)(int samples, int32_t *out);
    void (*poll)(void);
} zc_soil_probe_t;

typedef struct {
    int32_t dry, wet;
    uint32_t set;
} zc_soil_cal_blob_t;

static zc_soil_probe_t *s_zc_soil_probes[ZC_SOIL_CAL_MAX_PROBES];
static int s_zc_soil_probe_count = 0;

static int32_t zc_soil_percent(int32_t raw, int32_t dry, int32_t wet)
{
    if (wet == dry) return 0;
    int32_t pct = ((raw - dry) * 100 + (wet - dry) / 2) / (wet - dry);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

static void zc_soil_cal_key(const zc_soil_probe_t *p, char *key, size_t n)
{
    snprintf(key, n, "%.15s", p->name);  /* NVS keys are at most 15 characters */
}

static void zc_soil_cal_load(zc_soil_probe_t *p)
{
    p->dry = p->def_dry;
    p->wet = p->def_wet;
    p->set = 0;
    nvs_handle_t h;
    if (nvs_open(ZC_SOIL_CAL_NVS_NS, NVS_READONLY, &h) != ESP_OK) return;  /* nothing saved yet */
    char key[16];
    zc_soil_cal_key(p, key, sizeof(key));
    zc_soil_cal_blob_t b = {};
    size_t len = sizeof(b);
    if (nvs_get_blob(h, key, &b, &len) == ESP_OK && len == sizeof(b) && b.dry != b.wet) {
        if (b.set & ZC_SOIL_CAL_DRY_SET) p->dry = b.dry;
        if (b.set & ZC_SOIL_CAL_WET_SET) p->wet = b.wet;
        p->set = b.set & (ZC_SOIL_CAL_DRY_SET | ZC_SOIL_CAL_WET_SET);
    }
    nvs_close(h);
}

static esp_err_t zc_soil_cal_store(const zc_soil_probe_t *p)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(ZC_SOIL_CAL_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    char key[16];
    zc_soil_cal_key(p, key, sizeof(key));
    if (p->set == 0) {
        err = nvs_erase_key(h, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    } else {
        zc_soil_cal_blob_t b = {};
        b.dry = p->dry;
        b.wet = p->wet;
        b.set = p->set;
        err = nvs_set_blob(h, key, &b, sizeof(b));
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static void zc_soil_cal_print(zc_soil_probe_t *p)
{
    int32_t raw = 0;
    int32_t dry = p->dry, wet = p->wet;
    if (!p->read_raw(ZC_SOIL_CAL_SAMPLES, &raw)) {
        printf("%s: no reading (the probe's pin has no working ADC channel)\n", p->name);
        return;
    }
    printf("%s: raw %ld, dry %ld (%s), wet %ld (%s) -> %ld %%\n", p->name, (long)raw,
           (long)dry, (p->set & ZC_SOIL_CAL_DRY_SET) ? "calibrated" : "default",
           (long)wet, (p->set & ZC_SOIL_CAL_WET_SET) ? "calibrated" : "default",
           (long)zc_soil_percent(raw, dry, wet));
}

static void zc_soil_cal_usage(void)
{
    printf("usage: soil-cal show|dry|wet|reset%s\n", s_zc_soil_probe_count > 1 ? " [probe]" : "");
    printf("  dry    probe in dry air: store its reading as 0 %%\n");
    printf("  wet    probe in water:   store its reading as 100 %%\n");
    printf("  show   raw reading now, the dry and wet points, percent\n");
    printf("  reset  back to the firmware's built-in points\n");
    if (s_zc_soil_probe_count > 1) {
        printf("probes:");
        for (int i = 0; i < s_zc_soil_probe_count; i++) printf(" %s", s_zc_soil_probes[i]->name);
        printf("\n");
    }
}

static int zc_soil_cal_cmd(int argc, char **argv)
{
    const char *verb = argc >= 2 ? argv[1] : "show";
    const char *which = argc >= 3 ? argv[2] : NULL;
    bool show = strcmp(verb, "show") == 0;
    bool dry = strcmp(verb, "dry") == 0;
    bool wet = strcmp(verb, "wet") == 0;
    bool reset = strcmp(verb, "reset") == 0;
    if (!(show || dry || wet || reset) || argc > 3) {
        zc_soil_cal_usage();
        return 1;
    }
    zc_soil_probe_t *p = NULL;
    if (which != NULL) {
        for (int i = 0; i < s_zc_soil_probe_count; i++) {
            if (strcasecmp(which, s_zc_soil_probes[i]->name) == 0) p = s_zc_soil_probes[i];
        }
        if (p == NULL) {
            printf("no probe named \"%s\"\n", which);
            zc_soil_cal_usage();
            return 1;
        }
    } else if (s_zc_soil_probe_count == 1) {
        p = s_zc_soil_probes[0];
    } else if (show) {
        for (int i = 0; i < s_zc_soil_probe_count; i++) zc_soil_cal_print(s_zc_soil_probes[i]);
        return 0;
    } else {
        printf("this device has %d probes; name one: soil-cal %s <probe>\n", s_zc_soil_probe_count, verb);
        zc_soil_cal_usage();
        return 1;
    }

    if (show) {
        zc_soil_cal_print(p);
        return 0;
    }
    if (reset) {
        p->dry = p->def_dry;
        p->wet = p->def_wet;
        p->set = 0;
    } else {
        int32_t raw = 0;
        if (!p->read_raw(ZC_SOIL_CAL_SAMPLES, &raw)) {
            printf("%s: no reading (the probe's pin has no working ADC channel) - nothing saved\n", p->name);
            return 1;
        }
        int32_t other = dry ? p->wet : p->dry;
        uint32_t other_set = dry ? ZC_SOIL_CAL_WET_SET : ZC_SOIL_CAL_DRY_SET;
        int32_t span = raw > other ? raw - other : other - raw;
        /* Against a point the owner set, too close is a mistake (the probe
         * did not move, or is not in air/water). Against a built-in default it
         * may just be a probe that reads the other way round, so only an
         * identical reading is refused there. */
        if (span == 0 || ((p->set & other_set) && span < ZC_SOIL_CAL_MIN_SPAN)) {
            printf("%s: reading %ld is too close to the %s point %ld (need %d apart) - nothing saved.\n"
                   "Put the probe %s and try again.\n",
                   p->name, (long)raw, dry ? "wet" : "dry", (long)other, ZC_SOIL_CAL_MIN_SPAN,
                   dry ? "in dry air (wiped dry)" : "in a glass of water up to its line");
            return 1;
        }
        if (dry) { p->dry = raw; p->set |= ZC_SOIL_CAL_DRY_SET; }
        else     { p->wet = raw; p->set |= ZC_SOIL_CAL_WET_SET; }
        if (span < ZC_SOIL_CAL_MIN_SPAN) {
            printf("%s: note: %ld is close to the built-in %s point; calibrate that one too.\n",
                   p->name, (long)raw, dry ? "wet" : "dry");
        }
    }
    esp_err_t err = zc_soil_cal_store(p);
    if (err != ESP_OK) {
        printf("%s: could not save to flash (%s) - in use until the next reboot\n", p->name, esp_err_to_name(err));
    }
    p->poll();  /* publish the new percent now, not at the next poll */
    zc_soil_cal_print(p);
    return err == ESP_OK ? 0 : 1;
}

static void zc_soil_cal_add(zc_soil_probe_t *p)
{
    zc_soil_cal_load(p);
    if (p->set) {
        ESP_LOGI(TAG, "soil probe %s: calibration from flash (dry=%ld wet=%ld)", p->name, (long)p->dry, (long)p->wet);
    }
    if (s_zc_soil_probe_count >= ZC_SOIL_CAL_MAX_PROBES) {
        ESP_LOGW(TAG, "soil probe %s: more than %d probes - not on soil-cal", p->name, ZC_SOIL_CAL_MAX_PROBES);
        return;
    }
    s_zc_soil_probes[s_zc_soil_probe_count++] = p;
    if (s_zc_soil_probe_count == 1) {
        /* Zero-init + assign: newer IDF adds fields to esp_console_cmd_t. */
        esp_console_cmd_t c = {};
        c.command = "soil-cal";
        c.help = "Calibrate the soil probe: soil-cal dry (in air) | wet (in water) | show | reset";
        c.func = &zc_soil_cal_cmd;
        esp_console_cmd_register(&c);
    }
}
#endif

/* Averaged raw reading of this probe; false when its ADC is not usable. */
static bool {{prefix_lc}}_soil_read_raw(int samples, int32_t *out)
{
    if (s_{{prefix_lc}}_adc == NULL) return false;  /* init failed and said so */
    if (samples < 1) samples = 1;
    if (samples > 64) samples = 64;
    int32_t sum = 0;
    int got = 0;
    for (int i = 0; i < samples; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_{{prefix_lc}}_adc, s_{{prefix_lc}}_adc_channel, &raw) == ESP_OK) {
            sum += raw;
            got++;
        }
    }
    if (got == 0) return false;
    *out = sum / got;
    return true;
}

static void {{prefix_lc}}_soil_poll(void);

static zc_soil_probe_t s_{{prefix_lc}}_soil_probe = {
    "{{prefix_lc}}", {{prefix}}_SOIL_DRY_RAW, {{prefix}}_SOIL_WET_RAW,
    {{prefix}}_SOIL_DRY_RAW, {{prefix}}_SOIL_WET_RAW, 0,
    &{{prefix_lc}}_soil_read_raw, &{{prefix_lc}}_soil_poll,
};

static void {{prefix_lc}}_soil_poll(void)
{
    int32_t raw = 0;
    if (!{{prefix_lc}}_soil_read_raw({{prefix}}_SOIL_SAMPLES, &raw)) return;
{{#if cfg.raw_param}}
    {
        app_driver_param_val_t r = { .u16 = (uint16_t)(raw < 0 ? 0 : raw) };
        app_driver_set_param({{cfg.raw_param}}, r, APP_DRIVER_SOURCE_LOCAL);
    }
{{/if}}
    /* The console may change the points between polls: read each once. */
    const int32_t dry = s_{{prefix_lc}}_soil_probe.dry;
    const int32_t wet = s_{{prefix_lc}}_soil_probe.wet;
    int32_t pct = zc_soil_percent(raw, dry, wet);
    ESP_LOGD(TAG, "soil probe {{prefix_lc}}: raw=%ld -> %ld %%", (long)raw, (long)pct);
    app_driver_param_val_t v = { .u8 = (uint8_t)pct };
    app_driver_set_param({{cfg.value_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}

static void {{prefix_lc}}_soil_poll_cb(void *arg)
{
    {{prefix_lc}}_soil_poll();
}
