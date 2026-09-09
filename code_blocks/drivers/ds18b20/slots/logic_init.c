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
    const esp_timer_create_args_t {{prefix_lc}}_args = {
        .callback = &{{prefix_lc}}_ds_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_ds",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, (uint64_t){{prefix}}_DS_POLL_MS * 1000ULL));
}
