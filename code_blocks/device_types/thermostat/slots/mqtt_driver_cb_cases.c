case {{cfg.local_temp_param}}: {
    char buf[16]; snprintf(buf, sizeof(buf), "%.2f", (double)val.i16 / 100.0);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_curr_topic, buf, 0, 1, true);
    return;
}
case {{cfg.heat_setpoint_param}}: {
    char buf[16]; snprintf(buf, sizeof(buf), "%.2f", (double)val.i16 / 100.0);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_temp_state_topic, buf, 0, 1, true);
    return;
}
