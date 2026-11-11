#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED
/* Written by the Wi-Fi event task, read by the timer: 64 bits on a 32-bit
 * CPU, so both sides go through the lock, and the check reads it ONCE — read
 * twice, a reconnect between the two reads made the outage the whole uptime
 * and rebooted a device that had just reconnected. */
static int64_t s_{{prefix_lc}}_disconnect_us = 0;
static portMUX_TYPE s_{{prefix_lc}}_disc_mux = portMUX_INITIALIZER_UNLOCKED;
static esp_timer_handle_t s_{{prefix_lc}}_recov_timer = NULL;

static void {{prefix_lc}}_recov_check(void *arg)
{
    portENTER_CRITICAL(&s_{{prefix_lc}}_disc_mux);
    int64_t since = s_{{prefix_lc}}_disconnect_us;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_disc_mux);
    if (since == 0) return;
    int64_t now = esp_timer_get_time();
    int64_t down_s = (now - since) / 1000000LL;
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
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_{{prefix_lc}}_disc_mux);
    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_{{prefix_lc}}_disconnect_us == 0) s_{{prefix_lc}}_disconnect_us = now;
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        s_{{prefix_lc}}_disconnect_us = 0;
    }
    portEXIT_CRITICAL(&s_{{prefix_lc}}_disc_mux);
}
#endif /* Wi-Fi, native or hosted */
