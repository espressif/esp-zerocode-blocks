{
    /* Nothing starts until the device is on the bus. A poll timer
       ticking against a handle that was never created would log one
       transfer error per period forever; one line at boot is the
       report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "BQ27220 {{prefix_lc}}: not started — no I2C bus");
    } else {
        /* Probe with a real read: the gauge has no chip-id register, so a
           successful state-of-charge read IS the presence test. No battery fitted
           is a degraded state, not a boot blocker — a Mosaico on USB alone is a
           perfectly normal way to run one. */
        uint16_t {{prefix_lc}}_probe = 0;
        if ({{prefix_lc}}_rd16({{prefix}}_GAUGE_REG_SOC, &{{prefix_lc}}_probe) != ESP_OK) {
            ESP_LOGW(TAG, "{{prefix_lc}}: no BQ27220 at 0x%02x — battery reporting disabled",
                     {{prefix}}_GAUGE_ADDR);
        } else {
            s_{{prefix_lc}}_present = true;
            const esp_timer_create_args_t {{prefix_lc}}_args = {
                .callback = &{{prefix_lc}}_poll_cb,
                .arg = NULL,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "{{prefix_lc}}_gauge",
                .skip_unhandled_events = true,
            };
            esp_timer_handle_t {{prefix_lc}}_timer = NULL;
            ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
            ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer,
                            (uint64_t){{prefix}}_GAUGE_POLL_MS * 1000ULL));
            /* Publish once immediately — a panel that shows no battery for the
               first 30 seconds looks broken. */
            {{prefix_lc}}_poll_cb(NULL);
            ESP_LOGI(TAG, "BQ27220 {{prefix_lc}}: I2C port %d addr 0x%02x",
                     {{prefix}}_GAUGE_PORT, {{prefix}}_GAUGE_ADDR);
        }
    }
}
