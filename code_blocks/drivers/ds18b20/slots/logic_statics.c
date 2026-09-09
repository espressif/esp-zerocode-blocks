extern onewire_bus_handle_t {{cfg.bus_handle_fn}}(void);
static ds18b20_device_handle_t s_{{prefix_lc}}_ds = NULL;

static void {{prefix_lc}}_ds_poll_cb(void *arg)
{
    if (!s_{{prefix_lc}}_ds) return;
    if (ds18b20_trigger_temperature_conversion(s_{{prefix_lc}}_ds) != ESP_OK) return;
    vTaskDelay(pdMS_TO_TICKS(800));
    float t_c = 0.0f;
    if (ds18b20_get_temperature(s_{{prefix_lc}}_ds, &t_c) != ESP_OK) return;
    int16_t t_x100 = (int16_t)(t_c * 100.0f);
    app_driver_param_val_t v = { .i16 = t_x100 };
    app_driver_set_param({{cfg.temp_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
