static char s_{{prefix_lc}}_state_topic[72] = {0};

static void {{prefix_lc}}_mqtt_publish_value(int16_t raw)
{
    const int scale = {{cfg.scale}};
    const int dp = scale >= 1000 ? 3 : scale >= 100 ? 2 : scale >= 10 ? 1 : 0;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.*f", dp, (double)raw / scale);
    zc_mqtt_publish(s_{{prefix_lc}}_state_topic, buf, 0, 1, true);
}

static void {{prefix_lc}}_mqtt_publish_reading(app_driver_param_val_t val)
{
    {{prefix_lc}}_mqtt_publish_value(val.i16);
}

/* False only while the driver says the reading is not valid (status_param). */
static bool {{prefix_lc}}_reading_valid(void)
{
{{#if cfg.status_param}}    app_driver_param_val_t st = {};
    if (app_driver_param_seen({{cfg.status_param}}) &&
        app_driver_get_param({{cfg.status_param}}, &st) == ESP_OK && !st.b) {
        return false;
    }
{{/if}}    return true;
}
