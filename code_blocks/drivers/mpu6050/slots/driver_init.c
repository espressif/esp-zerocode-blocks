{
    /* Nothing starts until the device is on the bus (see imu_bmi270). */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 {{prefix_lc}}: not started — no I2C bus");
    } else {
        /* A missing accelerometer at boot is a degraded state, not a boot
           blocker: the poll keeps re-probing and configures it when it answers.
           configure() says why it failed, once. */
        (void){{prefix_lc}}_configure();
        s_{{prefix_lc}}_present = true;
        /* On the bus's poll task, not the shared esp_timer task: the read and
           the re-probe wait for the bus (app_driver_poll_every, app_driver.h). */
        if (app_driver_poll_every(&{{prefix_lc}}_poll_cb, NULL, (uint32_t){{prefix}}_IMU_POLL_MS) != ESP_OK) {
            ESP_LOGE(TAG, "MPU6050 {{prefix_lc}}: could not start polling");
        }
        ESP_LOGI(TAG, "MPU6050 {{prefix_lc}}: I2C port %d addr 0x%02x, %d ms poll",
                 {{prefix}}_IMU_PORT, {{prefix}}_IMU_ADDR, {{prefix}}_IMU_POLL_MS);
    }
}
