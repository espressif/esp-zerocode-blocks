{
    /* Nothing starts until the device is on the bus. A poll timer
       ticking against a handle that was never created would log one
       transfer error per period forever; one line at boot is the
       report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "SHT3x {{prefix_lc}}: not started — no I2C bus");
    } else {
        /* On the bus's poll task, not the shared esp_timer task: the read waits
           for the bus and the device (app_driver_poll_every, app_driver.h). */
        if (app_driver_poll_every(&{{prefix_lc}}_sht3_poll_cb, NULL, (uint32_t)({{prefix}}_SHT3_POLL_MS)) != ESP_OK) {
            ESP_LOGE(TAG, "sht3x {{prefix_lc}}: could not start polling");
        }
        ESP_LOGI(TAG, "SHT3x {{prefix_lc}}: I2C port %d addr 0x%02x", {{prefix}}_SHT3_PORT, {{prefix}}_SHT3_ADDR);
    }
}
