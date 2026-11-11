extern onewire_bus_handle_t {{cfg.bus_handle_fn}}(void);
static onewire_bus_handle_t s_{{prefix_lc}}_bus = NULL;
static ds18b20_device_handle_t s_{{prefix_lc}}_ds = NULL;
static uint32_t s_{{prefix_lc}}_ds_failures = 0;
static uint32_t s_{{prefix_lc}}_ds_misses = 0;       /* searches in a row that found no probe */
static uint32_t s_{{prefix_lc}}_ds_next_search = 0;  /* polls until the next search while unbound */

/* ms as a whole number of polls, at least one. */
static uint32_t {{prefix_lc}}_ds_polls(uint32_t ms)
{
    uint32_t poll = (uint32_t){{prefix}}_DS_POLL_MS ? (uint32_t){{prefix}}_DS_POLL_MS : 1;
    uint32_t n = ms / poll;
    return n ? n : 1;
}

/* Search the bus and open the first device that is a DS18B20. Runs at boot
   and again, every rescan_interval_ms, for as long as no probe is bound
   — a probe plugged in late, or one that dropped off the bus, is picked up
   without a reboot. A miss is logged the first time, then once a minute. */
static bool {{prefix_lc}}_ds_bind(void)
{
    if (!s_{{prefix_lc}}_bus) return false;
    onewire_device_iter_handle_t iter = NULL;
    if (onewire_new_device_iter(s_{{prefix_lc}}_bus, &iter) == ESP_OK) {
        onewire_device_t dev;
        while (!s_{{prefix_lc}}_ds && onewire_device_iter_get_next(iter, &dev) == ESP_OK) {
            ds18b20_config_t dscfg = {};
            /* NOT ds18b20_new_device — espressif/ds18b20 0.2.0 renamed it (and
               ds18b20_new_single_device to ds18b20_new_device_from_bus). This
               block pins ^0.4.0. A device that is not a DS18B20 is refused
               here, and the search moves on to the next one. */
            if (ds18b20_new_device_from_enumeration(&dev, &dscfg, &s_{{prefix_lc}}_ds) != ESP_OK) {
                s_{{prefix_lc}}_ds = NULL;
            }
        }
        onewire_del_device_iter(iter);
    }
    if (s_{{prefix_lc}}_ds) {
        ds18b20_set_resolution(s_{{prefix_lc}}_ds, DS18B20_RESOLUTION_12B);
        if (s_{{prefix_lc}}_ds_misses) {
            ESP_LOGI(TAG, "DS18B20 {{prefix_lc}}: probe found after %lu search(es)", (unsigned long)(s_{{prefix_lc}}_ds_misses + 1));
        } else {
            ESP_LOGI(TAG, "DS18B20 {{prefix_lc}}: probe found");
        }
        s_{{prefix_lc}}_ds_misses = 0;
        return true;
    }
    s_{{prefix_lc}}_ds_misses++;
    uint32_t every = {{prefix_lc}}_ds_polls({{prefix}}_DS_RESCAN_MS);
    uint32_t per_minute = {{prefix_lc}}_ds_polls(60000UL) / every;
    if (per_minute == 0) per_minute = 1;
    if (s_{{prefix_lc}}_ds_misses == 1 || (s_{{prefix_lc}}_ds_misses % per_minute) == 0) {
        ESP_LOGW(TAG, "DS18B20 {{prefix_lc}}: no probe found on the bus, searching again every %lu ms",
                 (unsigned long)(every * (uint32_t){{prefix}}_DS_POLL_MS));
    }
    s_{{prefix_lc}}_ds_next_search = every;
    return false;
}

static void {{prefix_lc}}_ds_status(bool ok)
{
{{#if cfg.status_param}}
    app_driver_param_val_t sv = { .b = ok };
    app_driver_set_param({{cfg.status_param}}, sv, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
    (void)ok;
}

static void {{prefix_lc}}_ds_poll(void)
{
    if (!s_{{prefix_lc}}_ds) {
        if (s_{{prefix_lc}}_ds_next_search > 1) {
            s_{{prefix_lc}}_ds_next_search--;
        } else {
            {{prefix_lc}}_ds_bind();
        }
        if (!s_{{prefix_lc}}_ds) {
            {{prefix_lc}}_ds_status(false);
            return;
        }
    }
    /* trigger_temperature_conversion blocks for the conversion time itself
       (800 ms at 12-bit) — no extra delay here. */
    esp_err_t err = ds18b20_trigger_temperature_conversion(s_{{prefix_lc}}_ds);
    float t_c = 0.0f;
    if (err == ESP_OK) err = ds18b20_get_temperature(s_{{prefix_lc}}_ds, &t_c);
    if (err == ESP_OK) {
        if (s_{{prefix_lc}}_ds_failures) {
            ESP_LOGI(TAG, "DS18B20 {{prefix_lc}}: reading again after %lu failed poll(s)", (unsigned long)s_{{prefix_lc}}_ds_failures);
            s_{{prefix_lc}}_ds_failures = 0;
        }
        int16_t t_x100 = (int16_t)(t_c * 100.0f);
        app_driver_param_val_t v = { .i16 = t_x100 };
        app_driver_set_param({{cfg.temp_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        {{prefix_lc}}_ds_status(true);
        return;
    }
    /* A failed read publishes no temperature — the last value stays on the bus
       and status_param says it is stale. The first failure at WARN, then once
       a minute (same contract as aht21). */
    s_{{prefix_lc}}_ds_failures++;
    uint32_t per_minute = 60000UL / (uint32_t){{prefix}}_DS_POLL_MS;
    if (per_minute == 0) per_minute = 1;
    if (s_{{prefix_lc}}_ds_failures == 1 || (s_{{prefix_lc}}_ds_failures % per_minute) == 0) {
        ESP_LOGW(TAG, "DS18B20 {{prefix_lc}}: read failed (%s), %lu consecutive", esp_err_to_name(err), (unsigned long)s_{{prefix_lc}}_ds_failures);
    }
    /* A run of failures: let the handle go and search the bus again on the
       next poll — a probe re-plugged (or another probe on the same pin) has
       to be found again before it answers. */
    uint32_t rebind = {{prefix}}_DS_REBIND_AFTER > 0 ? (uint32_t){{prefix}}_DS_REBIND_AFTER : 1;
    if ((s_{{prefix_lc}}_ds_failures % rebind) == 0) {
        ds18b20_del_device(s_{{prefix_lc}}_ds);
        s_{{prefix_lc}}_ds = NULL;
        s_{{prefix_lc}}_ds_next_search = 0;
    }
    {{prefix_lc}}_ds_status(false);
}

static void {{prefix_lc}}_ds_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        {{prefix_lc}}_ds_poll();
        TickType_t period = pdMS_TO_TICKS({{prefix}}_DS_POLL_MS);
        vTaskDelayUntil(&last, period ? period : 1);
    }
}
