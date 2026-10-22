{
    onewire_bus_handle_t {{prefix_lc}}_bus = {{cfg.bus_handle_fn}}();
    if (!{{prefix_lc}}_bus) {
        ESP_LOGE(TAG, "{{prefix_lc}}: 1-Wire bus handle is NULL — peripheral not initialized?");
    } else {
        onewire_device_iter_handle_t {{prefix_lc}}_iter = NULL;
        ESP_ERROR_CHECK(onewire_new_device_iter({{prefix_lc}}_bus, &{{prefix_lc}}_iter));
        onewire_device_t {{prefix_lc}}_dev;
        if (onewire_device_iter_get_next({{prefix_lc}}_iter, &{{prefix_lc}}_dev) == ESP_OK) {
            ds18b20_config_t {{prefix_lc}}_dscfg = {};
            /* NOT ds18b20_new_device — espressif/ds18b20 0.2.0 renamed it (and
               ds18b20_new_single_device to ds18b20_new_device_from_bus). This
               block pins ^0.4.0, so the old name has never resolved; nothing
               caught it because no catalog product instantiated the block, so
               nothing ever compiled it. */
            if (ds18b20_new_device_from_enumeration(&{{prefix_lc}}_dev, &{{prefix_lc}}_dscfg,
                                                    &s_{{prefix_lc}}_ds) != ESP_OK) {
                /* Degraded, not fatal — the contract every driver block here
                   keeps: the product runs, this probe just does not report. */
                ESP_LOGE(TAG, "DS18B20 {{prefix_lc}}: device on the bus would not open");
                s_{{prefix_lc}}_ds = NULL;
            } else {
                ds18b20_set_resolution(s_{{prefix_lc}}_ds, DS18B20_RESOLUTION_12B);
                ESP_LOGI(TAG, "DS18B20 {{prefix_lc}}: probe found");
            }
        } else {
            ESP_LOGW(TAG, "DS18B20 {{prefix_lc}}: no probe found on bus");
        }
        onewire_del_device_iter({{prefix_lc}}_iter);
    }
    /* Its own task, NOT an esp_timer callback: a conversion takes 800 ms at
       12-bit and espressif/ds18b20 waits it out inside
       ds18b20_trigger_temperature_conversion(). On the shared esp_timer task
       that stalled every other timer in the product (button scanning, LED
       blinks, other sensors) for the whole conversion, every poll. */
    xTaskCreate({{prefix_lc}}_ds_task, "{{prefix_lc}}_ds", 3072, NULL, 3, NULL);
}
