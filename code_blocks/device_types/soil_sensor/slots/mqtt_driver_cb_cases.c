case {{cfg.value_param}}: {
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", (unsigned)val.u8);
    zc_mqtt_publish(s_{{prefix_lc}}_moisture_topic, buf, 0, 1, true);
    return;
}
