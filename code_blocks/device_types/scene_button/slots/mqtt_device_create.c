{
    snprintf(s_{{prefix_lc}}_trigger_topic, sizeof(s_{{prefix_lc}}_trigger_topic),
             "zerocode/%s/{{prefix_lc}}/trigger", s_node_id);

    /* Controllers are INPUTS: they surface in HA as device triggers you build
     * automations on, not as stateful entities. Requires the shared device
     * object — HA attaches the trigger to the node's device. */
    char disc_topic[104];
    snprintf(disc_topic, sizeof(disc_topic),
             "homeassistant/device_automation/zc_%s_{{prefix_lc}}/config", s_node_id);
    char cfg_json[512];
    snprintf(cfg_json, sizeof(cfg_json),
             "{\"automation_type\":\"trigger\",\"topic\":\"%s\",\"payload\":\"press\","
             "\"type\":\"button_short_press\",\"subtype\":\"button_1\","
             "\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"}}",
             s_{{prefix_lc}}_trigger_topic, s_node_id, s_node_id);
    esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    ESP_LOGI(TAG, "{{prefix_lc}}: HA device trigger announced");
}
