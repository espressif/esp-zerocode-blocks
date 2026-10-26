case {{cfg.value_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f", (double)val.u16 / 10.0);
    zc_mqtt_publish(s_{{prefix_lc}}_flow_topic, buf, 0, 1, true);
    return;
}
