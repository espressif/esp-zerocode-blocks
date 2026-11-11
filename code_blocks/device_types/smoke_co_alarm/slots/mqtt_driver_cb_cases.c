case {{cfg.smoke_state_param}}: {
    zc_mqtt_publish(s_{{prefix_lc}}_smoke_topic,
                    (val.u8 != 0) ? "ON" : "OFF", 0, 1, true);
    return;
}
case {{cfg.co_state_param}}: {
    zc_mqtt_publish(s_{{prefix_lc}}_co_topic,
                    (val.u8 != 0) ? "ON" : "OFF", 0, 1, true);
    return;
}
