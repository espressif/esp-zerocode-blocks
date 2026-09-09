case {{cfg.active_power_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", (double)val.u32 / 1000.0);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_power_topic, buf, 0, 1, true);
    return;
}
{{#if cfg.voltage_param}}
case {{cfg.voltage_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", (double)val.u32 / 1000.0);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_voltage_topic, buf, 0, 1, true);
    return;
}
{{/if}}
{{#if cfg.current_param}}
case {{cfg.current_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", (double)val.u32 / 1000.0);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_current_topic, buf, 0, 1, true);
    return;
}
{{/if}}
