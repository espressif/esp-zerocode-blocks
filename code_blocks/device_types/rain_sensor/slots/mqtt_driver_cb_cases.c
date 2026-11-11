case {{cfg.rain_param}}: {
    zc_mqtt_publish(s_{{prefix_lc}}_rain_topic,
                    (val.b) ? "ON" : "OFF", 0, 1, true);
    return;
}
