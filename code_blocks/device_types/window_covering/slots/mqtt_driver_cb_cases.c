case {{cfg.position_param}}: {
    char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)(val.u16 / 100));
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_pos_topic, buf, 0, 1, true);
    return;
}
