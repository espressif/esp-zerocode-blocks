{
    snprintf(s_{{prefix_lc}}_trigger_topic, sizeof(s_{{prefix_lc}}_trigger_topic),
             "zerocode/%s/{{prefix_lc}}/trigger", s_node_id);

    /* One trigger topic, three payloads: up / down (dial) and toggle (button).
     * Each payload needs its own discovery config for HA to offer it. */
    const struct { const char *payload; const char *type; const char *subtype; } trigs[] = {
        { "up",     "brightness_move_up",   "dial" },
        { "down",   "brightness_move_down", "dial" },
        { "toggle", "button_short_press",   "button_1" },
    };
    for (size_t i = 0; i < sizeof(trigs) / sizeof(trigs[0]); i++) {
        char disc_topic[112];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/device_automation/zc_%s_{{prefix_lc}}_%s/config", s_node_id, trigs[i].payload);
        char cfg_json[512];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"automation_type\":\"trigger\",\"topic\":\"%s\",\"payload\":\"%s\","
                 "\"type\":\"%s\",\"subtype\":\"%s\","
                 "\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"}}",
                 s_{{prefix_lc}}_trigger_topic, trigs[i].payload, trigs[i].type, trigs[i].subtype,
                 s_node_id, s_node_id);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    ESP_LOGI(TAG, "{{prefix_lc}}: HA device triggers announced");
}
