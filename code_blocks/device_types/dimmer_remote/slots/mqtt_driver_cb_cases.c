case {{cfg.power_param}}: {
    zc_mqtt_publish(s_{{prefix_lc}}_trigger_topic, "toggle", 0, 1, false);
    return;
}
case {{cfg.level_param}}: {
    if (s_{{prefix_lc}}_mq_level_seen) {
        int8_t delta = (int8_t)(val.u8 - s_{{prefix_lc}}_mq_prev_level);
        if (delta > 0) zc_mqtt_publish(s_{{prefix_lc}}_trigger_topic, "up", 0, 1, false);
        else if (delta < 0) zc_mqtt_publish(s_{{prefix_lc}}_trigger_topic, "down", 0, 1, false);
    }
    s_{{prefix_lc}}_mq_prev_level = val.u8;
    s_{{prefix_lc}}_mq_level_seen = true;
    return;
}
