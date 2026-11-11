{
    /* Nothing starts until the device is on the bus. A poll timer
       ticking against a handle that was never created would log one
       transfer error per period forever; one line at boot is the
       report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "BMI270 {{prefix_lc}}: not started — no I2C bus");
    } else {
        /* Probe, configure and read on the bus's poll task (app_driver_poll_every):
           the bring-up waits, and it is retried until the part answers. */
        if (app_driver_poll_every(&{{prefix_lc}}_poll_cb, NULL, (uint32_t){{prefix}}_IMU_POLL_MS) != ESP_OK) {
            ESP_LOGE(TAG, "BMI270 {{prefix_lc}}: could not start polling");
        } else {
            ESP_LOGI(TAG, "BMI270 {{prefix_lc}}: I2C port %d addr 0x%02x, %d ms poll",
                     {{prefix}}_IMU_PORT, {{prefix}}_IMU_ADDR, {{prefix}}_IMU_POLL_MS);
        }
    }
}
