case {{cfg.position_param}}: {
    /* Any position change = a press event (not retained — events, not state). */
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_trigger_topic, "press", 0, 1, false);
    return;
}
