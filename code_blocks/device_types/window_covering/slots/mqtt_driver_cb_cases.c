case {{cfg.position_param}}: {
    char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)(val.u16 / 100));
    zc_mqtt_publish(s_{{prefix_lc}}_pos_topic, buf, 0, 1, true);
    return;
}
