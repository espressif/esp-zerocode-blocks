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
            /* Advanced power save OFF first. PWR_CONF resets to 0x03, and with
               adv_power_save set the part needs >= 450 us between register
               writes — the back-to-back ACC_CONF/ACC_RANGE pair below could
               drop the range write and leave the reset +/-8 g, against a
               16384 LSB/g scale: |a| ~250 mg, motion stuck true. A busy-wait,
               not vTaskDelay: 1 ms is under one tick at 100 Hz. */
            {{prefix_lc}}_wr({{prefix}}_IMU_REG_PWR_CONF, 0x00);
            esp_rom_delay_us(1000);
            {{prefix_lc}}_wr({{prefix}}_IMU_REG_PWR_CTRL, 0x04);
            /* 10 ms, not 5: pdMS_TO_TICKS() rounds DOWN, and under one tick
               (10 ms at the default 100 Hz) it is 0 — no wait at all. */
            vTaskDelay(pdMS_TO_TICKS(10));
            /* 100 Hz ODR, normal filter. Polling is slower than this on purpose —
               the sensor averages, we sample. */
            {{prefix_lc}}_wr({{prefix}}_IMU_REG_ACC_CONF, 0xA8);
            {{prefix_lc}}_wr({{prefix}}_IMU_REG_ACC_RANGE, 0x00);  /* +/-2 g */
            vTaskDelay(pdMS_TO_TICKS(10));
            /* Read the range back: every mg figure below assumes +/-2 g, and a
               write that did not land would read as motion forever with
               nothing in the log. Refusing to start is the honest degraded
               state. */
            uint8_t {{prefix_lc}}_range = 0xFF;
            if ({{prefix_lc}}_rd({{prefix}}_IMU_REG_ACC_RANGE, &{{prefix_lc}}_range, 1) != ESP_OK ||
                ({{prefix_lc}}_range & 0x03) != 0x00) {
                ESP_LOGE(TAG, "{{prefix_lc}}: BMI270 ACC_RANGE reads 0x%02x, not +/-2 g — motion disabled",
                         {{prefix_lc}}_range);
            } else {
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
}
