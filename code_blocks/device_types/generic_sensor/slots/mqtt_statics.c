static char s_{{prefix_lc}}_state_topic[72] = {0};

static void {{prefix_lc}}_mqtt_publish_value(int16_t raw)
{
    const int scale = {{cfg.scale}};
    const int dp = scale >= 1000 ? 3 : scale >= 100 ? 2 : scale >= 10 ? 1 : 0;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.*f", dp, (double)raw / scale);
    zc_mqtt_publish(s_{{prefix_lc}}_state_topic, buf, 0, 1, true);
}
