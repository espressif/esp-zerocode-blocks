case {{cfg.air_quality_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%u", (unsigned)val.u8);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_aqi_topic, buf, 0, 1, true);
    return;
}
{{#if cfg.co2_ppm_param}}
case {{cfg.co2_ppm_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%u", (unsigned)val.u16);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_co2_topic, buf, 0, 1, true);
    return;
}
{{/if}}
{{#if cfg.pm25_param}}
case {{cfg.pm25_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%u", (unsigned)val.u16);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_pm25_topic, buf, 0, 1, true);
    return;
}
{{/if}}
{{#if cfg.tvoc_param}}
case {{cfg.tvoc_param}}: {
    char buf[24];
    snprintf(buf, sizeof(buf), "%u", (unsigned)val.u16);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_tvoc_topic, buf, 0, 1, true);
    return;
}
{{/if}}
