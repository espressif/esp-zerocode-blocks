{
    /* The timezone applies to localtime() whether or not a sync ever lands. */
    setenv("TZ", {{prefix}}_TZ, 1);
    tzset();
#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED || CONFIG_ETH_ENABLED
    if (esp_sntp_enabled()) {
        /* A transport already runs SNTP (RainMaker does). Don't start a second
         * client — just observe its syncs. */
        esp_sntp_set_time_sync_notification_cb({{prefix_lc}}_sntp_synced);
        ESP_LOGI(TAG, "{{prefix_lc}}: SNTP already running — observing");
    } else {
        esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG({{prefix}}_NTP_SERVER);
        cfg.start = true;              /* the client waits for an IP by itself */
        cfg.sync_cb = {{prefix_lc}}_sntp_synced;
        esp_err_t err = esp_netif_sntp_init(&cfg);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "{{prefix_lc}}: SNTP init failed: %s — time() stays at the epoch", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "{{prefix_lc}}: SNTP started (%s)", {{prefix}}_NTP_SERVER);
        }
    }
#else
    ESP_LOGW(TAG, "{{prefix_lc}}: no IP stack on this chip — clock stays unset");
#endif
}
