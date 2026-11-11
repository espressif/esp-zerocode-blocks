static char s_{{prefix_lc}}_flow_topic[72] = {0};

/* One reading as Home Assistant's state payload. */
static void {{prefix_lc}}_mqtt_publish_reading(app_driver_param_val_t val)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f", (double)val.u16 / 10.0);
    zc_mqtt_publish(s_{{prefix_lc}}_flow_topic, buf, 0, 1, true);
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
