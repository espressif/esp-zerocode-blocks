{
    /* Nothing starts until the device is on the bus. A poll timer
       ticking against a handle that was never created would log one
       transfer error per period forever; one line at boot is the
       report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "SGP40 {{prefix_lc}}: not started — no I2C bus");
    } else {
        const esp_timer_create_args_t {{prefix_lc}}_args = {
            .callback = &{{prefix_lc}}_sgp_poll_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "{{prefix_lc}}_sgp",
            .skip_unhandled_events = true,
        };
        esp_timer_handle_t {{prefix_lc}}_timer = NULL;
        ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, (uint64_t){{prefix}}_SGP_POLL_MS * 1000ULL));
        ESP_LOGI(TAG, "SGP40 {{prefix_lc}}: I2C port %d addr 0x%02x", {{prefix}}_SGP_PORT, {{prefix}}_SGP_ADDR);
    }
}
