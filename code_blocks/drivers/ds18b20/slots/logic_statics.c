extern onewire_bus_handle_t {{cfg.bus_handle_fn}}(void);
static ds18b20_device_handle_t s_{{prefix_lc}}_ds = NULL;
static uint32_t s_{{prefix_lc}}_ds_failures = 0;

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
        {{prefix_lc}}_ds_status(false);
        return;
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
    /* A failed read leaves the last value on the bus but says so: the first
       failure at WARN, then once a minute (same contract as aht21). */
    s_{{prefix_lc}}_ds_failures++;
    uint32_t per_minute = 60000UL / (uint32_t){{prefix}}_DS_POLL_MS;
    if (per_minute == 0) per_minute = 1;
    if (s_{{prefix_lc}}_ds_failures == 1 || (s_{{prefix_lc}}_ds_failures % per_minute) == 0) {
        ESP_LOGW(TAG, "DS18B20 {{prefix_lc}}: read failed (%s), %lu consecutive", esp_err_to_name(err), (unsigned long)s_{{prefix_lc}}_ds_failures);
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
