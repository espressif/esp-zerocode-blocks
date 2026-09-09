{
    /* Nothing starts until the device is on the bus. A poll timer
       ticking against a handle that was never created would log one
       transfer error per period forever; one line at boot is the
       report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "BMM150 {{prefix_lc}}: not started — no I2C bus");
    } else {
        /* A BMM150 wakes in SUSPEND and answers nothing — not even CHIP_ID — until
           bit0 of the power register is set. Probing first would report every
           working sensor as absent, so power up, wait, THEN identify. */
        {{prefix_lc}}_wr({{prefix}}_MAG_REG_POWER, 0x01);
        vTaskDelay(pdMS_TO_TICKS(5));

        uint8_t {{prefix_lc}}_id = 0;
        if ({{prefix_lc}}_rd({{prefix}}_MAG_REG_CHIP_ID, &{{prefix_lc}}_id, 1) != ESP_OK ||
            {{prefix_lc}}_id != {{prefix}}_MAG_CHIP_ID_VAL) {
            ESP_LOGE(TAG, "{{prefix_lc}}: no BMM150 at 0x%02x (chip id 0x%02x)",
                     {{prefix}}_MAG_ADDR, {{prefix_lc}}_id);
        } else {
            /* Normal mode, 10 Hz. The sensor samples faster than we poll on
               purpose — it averages internally, we just take the latest. */
            {{prefix_lc}}_wr({{prefix}}_MAG_REG_OPMODE, 0x00);
            vTaskDelay(pdMS_TO_TICKS(5));
            s_{{prefix_lc}}_present_dev = true;

            const esp_timer_create_args_t {{prefix_lc}}_args = {
                .callback = &{{prefix_lc}}_poll_cb,
                .arg = NULL,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "{{prefix_lc}}_mag",
                .skip_unhandled_events = true,
            };
            esp_timer_handle_t {{prefix_lc}}_timer = NULL;
            ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
            ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer,
                            (uint64_t){{prefix}}_MAG_POLL_MS * 1000ULL));
            ESP_LOGI(TAG, "BMM150 {{prefix_lc}}: I2C port %d addr 0x%02x",
                     {{prefix}}_MAG_PORT, {{prefix}}_MAG_ADDR);
        }
    }
}
