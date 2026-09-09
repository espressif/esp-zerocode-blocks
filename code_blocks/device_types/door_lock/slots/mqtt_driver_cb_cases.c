case {{cfg.locked_param}}: {
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic,
                            val.b ? "LOCKED" : "UNLOCKED", 0, 1, true);
    return;
}
