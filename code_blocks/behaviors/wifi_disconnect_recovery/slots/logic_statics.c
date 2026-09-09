#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED
static int64_t s_{{prefix_lc}}_disconnect_us = 0;
static esp_timer_handle_t s_{{prefix_lc}}_recov_timer = NULL;

static void {{prefix_lc}}_recov_check(void *arg)
{
    if (s_{{prefix_lc}}_disconnect_us == 0) return;
    int64_t now = esp_timer_get_time();
    int64_t down_s = (now - s_{{prefix_lc}}_disconnect_us) / 1000000LL;
#if {{prefix}}_RECOV_TIMEOUT_S > 0
    if (down_s > (int64_t){{prefix}}_RECOV_TIMEOUT_S) {
        ESP_LOGW(TAG, "{{prefix_lc}}: Wi-Fi disconnected for %lld s — rebooting (reboot_after_seconds=%d)",
                 (long long)down_s, {{prefix}}_RECOV_TIMEOUT_S);
        esp_restart();
    }
#else
    /* reboot_after_seconds == 0: observe only. Local logic keeps running; the
     * Wi-Fi driver's own reconnect handles the radio. Logged once a minute so
     * the outage is visible in the console without spamming it. */
    if ((down_s % 60) < 30) {
        ESP_LOGW(TAG, "{{prefix_lc}}: Wi-Fi disconnected for %lld s (no reboot configured)", (long long)down_s);
    }
#endif
}

static void {{prefix_lc}}_wifi_event_cb(void *arg, esp_event_base_t event_base,
                                        int32_t event_id, void *event_data)
{
    if (event_base != WIFI_EVENT) return;
    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_{{prefix_lc}}_disconnect_us == 0) {
            s_{{prefix_lc}}_disconnect_us = esp_timer_get_time();
        }
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        s_{{prefix_lc}}_disconnect_us = 0;
    }
}
#endif /* Wi-Fi, native or hosted */
