case {{cfg.locked_param}}: {
    zc_mqtt_publish(s_{{prefix_lc}}_state_topic,
                    val.b ? "LOCKED" : "UNLOCKED", 0, 1, true);
    return;
}
