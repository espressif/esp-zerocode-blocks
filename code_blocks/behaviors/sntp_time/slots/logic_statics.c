#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED || CONFIG_ETH_ENABLED
static volatile bool s_{{prefix_lc}}_synced = false;

{{#if cfg.day_param}}
/* The local date as one number — days since 2000-01-01 in local time — so a
 * daily reset is "react when it changes". Published at the first sync and at
 * each local midnight after it; never before the clock is real, so a day can
 * never start at the 1970 epoch. */
static uint16_t {{prefix_lc}}_local_day(void)
{
    time_t now = time(NULL);
    struct tm lt = {};
    localtime_r(&now, &lt);
    int y = lt.tm_year + 1900, m = lt.tm_mon + 1, d = lt.tm_mday;
    /* days_from_civil (Howard Hinnant), relative to 2000-01-01 */
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153U * (unsigned)(m + (m > 2 ? -3 : 9)) + 2U) / 5U + (unsigned)d - 1U;
    unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    long days = (long)era * 146097L + (long)doe - 719468L - 10957L;
    return (uint16_t)(days < 0 ? 0 : days);
}

static void {{prefix_lc}}_day_check(void)
{
    if (!s_{{prefix_lc}}_synced) return;
    app_driver_param_val_t v = { .u16 = {{prefix_lc}}_local_day() };
    app_driver_set_param({{cfg.day_param}}, v, APP_DRIVER_SOURCE_LOCAL);   /* change-detected: listeners hear a new day only */
}

static void {{prefix_lc}}_day_tick(void *arg)
{
    (void)arg;
    {{prefix_lc}}_day_check();
}
{{/if}}

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
{{#if cfg.day_param}}
    {{prefix_lc}}_day_check();
{{/if}}
}
#endif
