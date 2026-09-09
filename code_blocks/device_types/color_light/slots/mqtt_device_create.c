{
    snprintf(s_{{prefix_lc}}_state_topic, sizeof(s_{{prefix_lc}}_state_topic),
             "zerocode/%s/{{prefix_lc}}/state", s_node_id);
    snprintf(s_{{prefix_lc}}_cmd_topic, sizeof(s_{{prefix_lc}}_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/set", s_node_id);
    {
        /* HA JSON schema — full color without a single value_template (the
         * schema knows the payload shape; jinja braces would collide with
         * the block template engine anyway). */
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/light/zc_%s_{{prefix_lc}}/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"schema\":\"json\",\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"brightness\":true,\"brightness_scale\":254,\"supported_color_modes\":[\"hs\"]}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_state_topic,
                 s_{{prefix_lc}}_cmd_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_cmd_topic, 1);
    s_{{prefix_lc}}_publish_state();
    ESP_LOGI(TAG, "{{prefix_lc}}: HA color light '{{cfg.label}}' announced (json schema)");
}
