{
    /* Nothing starts until the device is on the bus. A poll timer
       ticking against a handle that was never created would log one
       transfer error per period forever; one line at boot is the
       report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "SCD4x {{prefix_lc}}: not started — no I2C bus");
    } else {
        /* A warm restart leaves the sensor measuring, and it refuses a start
           while it measures: stop it first (0x3F86, then 500 ms before the
           next command, per the datasheet). A cold sensor NACKs the stop,
           which is harmless. Then start periodic measurement (0x21B1). */
        (void){{prefix_lc}}_scd_send_cmd(0x3F86);
        vTaskDelay(pdMS_TO_TICKS(500));
        {{prefix_lc}}_scd_send_cmd(0x21B1);
        /* On the bus's poll task, not the shared esp_timer task: the read waits
           for the bus and the device (app_driver_poll_every, app_driver.h). */
        if (app_driver_poll_every(&{{prefix_lc}}_scd_poll_cb, NULL, (uint32_t)({{prefix}}_SCD_POLL_MS)) != ESP_OK) {
            ESP_LOGE(TAG, "scd4x {{prefix_lc}}: could not start polling");
        }
        ESP_LOGI(TAG, "SCD4x {{prefix_lc}}: periodic measurement started");
    }
}
