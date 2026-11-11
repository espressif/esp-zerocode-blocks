{
    /* Nothing starts until the device is on the bus. A poll timer
       ticking against a handle that was never created would log one
       transfer error per period forever; one line at boot is the
       report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "AHT21 {{prefix_lc}}: not started — no I2C bus");
    } else {
        {{prefix_lc}}_aht_calibrate();
        /* On the bus's poll task, not the shared esp_timer task: the read waits
           for the bus and the device (app_driver_poll_every, app_driver.h). */
        if (app_driver_poll_every(&{{prefix_lc}}_aht_poll_cb, NULL, (uint32_t)({{prefix}}_AHT_POLL_MS)) != ESP_OK) {
            ESP_LOGE(TAG, "aht21 {{prefix_lc}}: could not start polling");
        }
        ESP_LOGI(TAG, "AHT21 {{prefix_lc}}: polling on I2C port %d addr 0x%02x", {{prefix}}_AHT_PORT, {{prefix}}_AHT_ADDR);
    }
}
