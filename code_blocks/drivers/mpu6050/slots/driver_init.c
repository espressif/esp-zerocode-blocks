{
    /* Nothing starts until the device is on the bus (see imu_bmi270). */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 {{prefix_lc}}: not started — no I2C bus");
    } else {
        /* A missing accelerometer at boot is a degraded state, not a boot
           blocker: the poll keeps re-probing and configures it when it answers. */
        if (!{{prefix_lc}}_configure()) {
            uint8_t id = 0;
            (void){{prefix_lc}}_rd({{prefix}}_IMU_REG_WHO_AM_I, &id, 1);
            ESP_LOGE(TAG, "{{prefix_lc}}: no MPU6050-family accelerometer at 0x%02x (WHO_AM_I 0x%02x) — will keep trying",
                     {{prefix}}_IMU_ADDR, id);
        }
        s_{{prefix_lc}}_present = true;
        const esp_timer_create_args_t {{prefix_lc}}_args = {
            .callback = &{{prefix_lc}}_poll_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "{{prefix_lc}}_imu",
            .skip_unhandled_events = true,
        };
        esp_timer_handle_t {{prefix_lc}}_timer = NULL;
        ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer,
                        (uint64_t){{prefix}}_IMU_POLL_MS * 1000ULL));
        ESP_LOGI(TAG, "MPU6050 {{prefix_lc}}: I2C port %d addr 0x%02x, %d ms poll",
                 {{prefix}}_IMU_PORT, {{prefix}}_IMU_ADDR, {{prefix}}_IMU_POLL_MS);
    }
}
