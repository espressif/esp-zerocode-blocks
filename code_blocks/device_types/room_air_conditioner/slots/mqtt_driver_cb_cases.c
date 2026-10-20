case {{cfg.cool_setpoint_param}}: {
    char buf[16]; snprintf(buf, sizeof(buf), "%.2f", (double)val.i16 / 100.0);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_temp_state_topic, buf, 0, 1, true);
    return;
}
case {{cfg.power_param}}: {
    /* Mirror a power change born anywhere else (Matter, RainMaker, a button,
     * voice) — the mode command path only publishes what IT was told. The AC
     * is cool-only, so power is the whole mode. */
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_mode_state_topic, val.b ? "cool" : "off", 0, 1, true);
    return;
}
