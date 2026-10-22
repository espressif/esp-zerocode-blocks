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
case {{cfg.system_mode_param}}: {
    /* Mirror a mode change born anywhere else (Matter, RainMaker, a button,
     * voice) — the mode command path only publishes what IT was told. The
     * endpoint is heat-only, so any non-Off mode reads as "heat". */
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_mode_state_topic, val.u8 == 0 ? "off" : "heat", 0, 1, true);
    return;
}
