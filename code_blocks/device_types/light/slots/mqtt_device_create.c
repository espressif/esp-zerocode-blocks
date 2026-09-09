{
    snprintf(s_{{prefix_lc}}_state_topic, sizeof(s_{{prefix_lc}}_state_topic),
             "zerocode/%s/{{prefix_lc}}/state", s_node_id);
    snprintf(s_{{prefix_lc}}_cmd_topic, sizeof(s_{{prefix_lc}}_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/set", s_node_id);

    /* Retained HA discovery config. Plain payloads on purpose — HA parses
     * ON/OFF without a value_template (jinja braces collide with the block
     * template engine). */
    char disc_topic[80];
    snprintf(disc_topic, sizeof(disc_topic),
             "homeassistant/light/zc_%s_{{prefix_lc}}/config", s_node_id);
    char cfg_json[512];
    snprintf(cfg_json, sizeof(cfg_json),
             "{\"name\":\"{{cfg.label}}\",\"uniq_id\":\"zc_%s_{{prefix_lc}}\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},"
             "\"stat_t\":\"%s\",\"cmd_t\":\"%s\",\"avty_t\":\"%s\","
             "\"pl_on\":\"ON\",\"pl_off\":\"OFF\"}",
             s_node_id, s_node_id, s_node_id, s_{{prefix_lc}}_state_topic, s_{{prefix_lc}}_cmd_topic, s_status_topic);
    esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);

    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_cmd_topic, 1);

    /* Current state, so HA is right immediately (also heals reconnects). */
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.power_param}}, &init);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic,
                            init.b ? "ON" : "OFF", 0, 1, true);
    ESP_LOGI(TAG, "{{prefix_lc}}: HA light '{{cfg.label}}' announced");
}
