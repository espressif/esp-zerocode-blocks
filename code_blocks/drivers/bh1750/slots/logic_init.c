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
        /* On the bus's poll task, not the shared esp_timer task: the read waits
           for the bus and the device (app_driver_poll_every, app_driver.h). */
        if (app_driver_poll_every(&{{prefix_lc}}_bh_poll_cb, NULL, (uint32_t)({{prefix}}_BH_POLL_MS)) != ESP_OK) {
            ESP_LOGE(TAG, "bh1750 {{prefix_lc}}: could not start polling");
        }
        ESP_LOGI(TAG, "BH1750 {{prefix_lc}}: continuous-high-res mode started", "");
    }
}
