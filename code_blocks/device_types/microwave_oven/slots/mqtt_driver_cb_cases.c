case {{cfg.cook_time_param}}: {
    char buf[16]; snprintf(buf, sizeof(buf), "%u", (unsigned)val.u32);
    zc_mqtt_publish(s_{{prefix_lc}}_cooktime_state_topic, buf, 0, 1, true);
    return;
}
case {{cfg.power_level_param}}: {
    char buf[16]; snprintf(buf, sizeof(buf), "%u", (unsigned)val.u8);
    zc_mqtt_publish(s_{{prefix_lc}}_power_state_topic, buf, 0, 1, true);
    return;
}
