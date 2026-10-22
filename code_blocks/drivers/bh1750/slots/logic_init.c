{
    /* Nothing starts until the device is on the bus. A poll timer
       ticking against a handle that was never created would log one
       transfer error per period forever; one line at boot is the
       report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 {{prefix_lc}}: not started — no I2C bus");
    } else {
        esp_err_t {{prefix_lc}}_arm_err = {{prefix_lc}}_bh_arm();
        if ({{prefix_lc}}_arm_err != ESP_OK) {
            /* Not fatal: every failed poll re-arms. Say so once, now. */
            ESP_LOGW(TAG, "BH1750 {{prefix_lc}}: mode command failed (%s) — retrying each poll",
                     esp_err_to_name({{prefix_lc}}_arm_err));
        }
        const esp_timer_create_args_t {{prefix_lc}}_args = {
            .callback = &{{prefix_lc}}_bh_poll_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "{{prefix_lc}}_bh",
            .skip_unhandled_events = true,
        };
        esp_timer_handle_t {{prefix_lc}}_timer = NULL;
        ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, (uint64_t){{prefix}}_BH_POLL_MS * 1000ULL));
        ESP_LOGI(TAG, "BH1750 {{prefix_lc}}: continuous-high-res mode started", "");
    }
}
