case {{cfg.power_param}}: {
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic, val.b ? "ON" : "OFF", 0, 1, true);
    return;
}
case {{cfg.brightness_param}}: {
    char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)val.u8);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_bri_state_topic, buf, 0, 1, true);
    return;
}
case {{cfg.color_temp_param}}: {
    char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)val.u16);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_ct_state_topic, buf, 0, 1, true);
    return;
}
