#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED || CONFIG_ETH_ENABLED
static bool s_{{prefix_lc}}_synced = false;

/* Runs on the SNTP task, once per successful sync. Only the first one matters
 * to the product: after it, time() is trustworthy. */
static void {{prefix_lc}}_sntp_synced(struct timeval *tv)
{
    (void)tv;
    if (s_{{prefix_lc}}_synced) return;
    s_{{prefix_lc}}_synced = true;
    time_t now = time(NULL);
    struct tm lt = {};
    localtime_r(&now, &lt);
    char buf[32] = {0};
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
    ESP_LOGI(TAG, "{{prefix_lc}}: clock synced — local time %s (TZ %s)", buf, {{prefix}}_TZ);
{{#if cfg.valid_param}}
    app_driver_param_val_t v = { .b = true };
    app_driver_set_param({{cfg.valid_param}}, v, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
}
#endif
