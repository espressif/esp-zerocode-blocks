case {{cfg.freeze_param}}: {
    zc_mqtt_publish(s_{{prefix_lc}}_freeze_topic,
                    (val.b) ? "ON" : "OFF", 0, 1, true);
    return;
}
