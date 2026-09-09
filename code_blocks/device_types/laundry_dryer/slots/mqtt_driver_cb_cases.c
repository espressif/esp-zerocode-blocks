case {{cfg.dryness_param}}: {
    if (val.u8 < 4)
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic,
                                s_{{prefix_lc}}_options[val.u8], 0, 1, true);
    return;
}
