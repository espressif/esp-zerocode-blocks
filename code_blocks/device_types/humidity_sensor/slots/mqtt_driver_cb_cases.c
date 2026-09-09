case {{cfg.value_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", (double)val.i16 / 100.0);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_humidity_topic, buf, 0, 1, true);
    return;
}
