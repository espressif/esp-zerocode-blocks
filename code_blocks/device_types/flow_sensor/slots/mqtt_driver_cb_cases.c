case {{cfg.value_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f", (double)val.u16 / 10.0);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_flow_topic, buf, 0, 1, true);
    return;
}
