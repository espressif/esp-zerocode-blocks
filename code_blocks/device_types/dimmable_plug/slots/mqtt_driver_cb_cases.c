case {{cfg.power_param}}: {
    zc_mqtt_publish(s_{{prefix_lc}}_state_topic, val.b ? "ON" : "OFF", 0, 1, true);
    return;
}
case {{cfg.brightness_param}}: {
    char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)val.u8);
    zc_mqtt_publish(s_{{prefix_lc}}_bri_state_topic, buf, 0, 1, true);
    return;
}
