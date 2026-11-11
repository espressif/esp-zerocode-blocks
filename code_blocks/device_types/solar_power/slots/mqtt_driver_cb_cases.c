case {{cfg.power_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", (double)val.u32 / 1000.0);
    zc_mqtt_publish(s_{{prefix_lc}}_power_topic, buf, 0, 1, true);
    return;
}
case {{cfg.energy_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%u", (unsigned)val.u32);
    zc_mqtt_publish(s_{{prefix_lc}}_energy_topic, buf, 0, 1, true);
    return;
}
