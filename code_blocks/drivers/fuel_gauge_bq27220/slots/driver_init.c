{
    /* Nothing starts until the device is on the bus. A poll timer
       ticking against a handle that was never created would log one
       transfer error per period forever; one line at boot is the
       report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "BQ27220 {{prefix_lc}}: not started — no I2C bus");
    } else {
        /* No battery fitted is a degraded state, not a boot blocker (a Mosaico on
           USB alone is a normal way to run one). The poll's own read is the
           presence test, on the bus's poll task, from now on — so a gauge that
           answers later is picked up. The first poll runs at once: a panel that
           shows no battery for the first 30 seconds looks broken. */
        if (app_driver_poll_every(&{{prefix_lc}}_poll_cb, NULL, (uint32_t){{prefix}}_GAUGE_POLL_MS) != ESP_OK) {
            ESP_LOGE(TAG, "BQ27220 {{prefix_lc}}: could not start polling");
        } else {
            ESP_LOGI(TAG, "BQ27220 {{prefix_lc}}: I2C port %d addr 0x%02x",
                     {{prefix}}_GAUGE_PORT, {{prefix}}_GAUGE_ADDR);
        }
    }
}
