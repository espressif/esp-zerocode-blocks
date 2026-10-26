case {{cfg.value_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", (double)val.i16 / 100.0);
    zc_mqtt_publish(s_{{prefix_lc}}_humidity_topic, buf, 0, 1, true);
    return;
}
