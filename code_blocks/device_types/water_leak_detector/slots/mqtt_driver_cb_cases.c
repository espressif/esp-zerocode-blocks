case {{cfg.leak_param}}: {
    zc_mqtt_publish(s_{{prefix_lc}}_leak_topic,
                    (val.b) ? "ON" : "OFF", 0, 1, true);
    return;
}
