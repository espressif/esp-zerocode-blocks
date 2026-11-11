case {{cfg.speed_param}}: {
    zc_mqtt_publish(s_{{prefix_lc}}_state_topic, val.u8 > 0 ? "ON" : "OFF", 0, 1, true);
    char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)val.u8);
    zc_mqtt_publish(s_{{prefix_lc}}_pct_state_topic, buf, 0, 1, true);
    return;
}
