{
    /* Nothing starts until the device is on the bus. A poll timer
       ticking against a handle that was never created would log one
       transfer error per period forever; one line at boot is the
       report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "BMI270 {{prefix_lc}}: not started — no I2C bus");
    } else {
        /* Probe before configuring: a missing IMU is a degraded state, not a boot
           blocker, and CHIP_ID is the only read that distinguishes "no device on
           the bus" from "device that did not like our config". */
        uint8_t {{prefix_lc}}_id = 0;
        if ({{prefix_lc}}_rd({{prefix}}_IMU_REG_CHIP_ID, &{{prefix_lc}}_id, 1) != ESP_OK ||
            {{prefix_lc}}_id != {{prefix}}_IMU_CHIP_ID_VAL) {
            ESP_LOGE(TAG, "{{prefix_lc}}: no BMI270 at 0x%02x (chip id 0x%02x) — motion disabled",
                     {{prefix}}_IMU_ADDR, {{prefix_lc}}_id);
        } else {
            /* Accelerometer on, gyro and temperature off — see block.yml: the gyro
               is what would need the licensed firmware blob, and nothing here reads
               it. Leaving it off also saves the power an always-on panel cares
               about. */
            {{prefix_lc}}_wr({{prefix}}_IMU_REG_PWR_CTRL, 0x04);
            vTaskDelay(pdMS_TO_TICKS(5));
            /* 100 Hz ODR, normal filter. Polling is slower than this on purpose —
               the sensor averages, we sample. */
            {{prefix_lc}}_wr({{prefix}}_IMU_REG_ACC_CONF, 0xA8);
            {{prefix_lc}}_wr({{prefix}}_IMU_REG_ACC_RANGE, 0x00);  /* +/-2 g */
            vTaskDelay(pdMS_TO_TICKS(5));
            s_{{prefix_lc}}_present = true;

            const esp_timer_create_args_t {{prefix_lc}}_args = {
                .callback = &{{prefix_lc}}_poll_cb,
                .arg = NULL,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "{{prefix_lc}}_imu",
                /* skip_unhandled_events: if the timer task is busy, drop the tick
                   rather than queue a burst of stale samples. */
                .skip_unhandled_events = true,
            };
            esp_timer_handle_t {{prefix_lc}}_timer = NULL;
            ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
            ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer,
                            (uint64_t){{prefix}}_IMU_POLL_MS * 1000ULL));
            ESP_LOGI(TAG, "BMI270 {{prefix_lc}}: I2C port %d addr 0x%02x, %d ms poll",
                     {{prefix}}_IMU_PORT, {{prefix}}_IMU_ADDR, {{prefix}}_IMU_POLL_MS);
        }
    }
}
