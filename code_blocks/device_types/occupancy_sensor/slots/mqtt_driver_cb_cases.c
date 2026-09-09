case {{cfg.occupied_param}}: {
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_occupancy_topic,
                            (val.b) ? "ON" : "OFF", 0, 1, true);
    return;
}
