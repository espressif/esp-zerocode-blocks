case {{cfg.speed_param}}: {
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic, val.u8 > 0 ? "ON" : "OFF", 0, 1, true);
    char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)val.u8);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_pct_state_topic, buf, 0, 1, true);
    return;
}
