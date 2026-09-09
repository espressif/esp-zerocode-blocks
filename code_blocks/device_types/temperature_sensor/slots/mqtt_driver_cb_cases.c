case {{cfg.value_param}}: {
    /* i16 hundredths of °C → bare decimal string; HA parses numbers. */
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", (double)val.i16 / 100.0);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic, buf, 0, 1, true);
    return;
}
