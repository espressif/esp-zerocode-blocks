{
    snprintf(s_{{prefix_lc}}_state_topic, sizeof(s_{{prefix_lc}}_state_topic),
             "zerocode/%s/{{prefix_lc}}/state", s_node_id);
    snprintf(s_{{prefix_lc}}_cmd_topic, sizeof(s_{{prefix_lc}}_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/cmd", s_node_id);
    snprintf(s_{{prefix_lc}}_pct_state_topic, sizeof(s_{{prefix_lc}}_pct_state_topic),
             "zerocode/%s/{{prefix_lc}}/pct/state", s_node_id);
    snprintf(s_{{prefix_lc}}_pct_cmd_topic, sizeof(s_{{prefix_lc}}_pct_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/pct/cmd", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/fan/zc_%s_{{prefix_lc}}/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"percentage_state_topic\":\"%s\",\"percentage_command_topic\":\"%s\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_state_topic,
                 s_{{prefix_lc}}_cmd_topic,
                 s_{{prefix_lc}}_pct_state_topic,
                 s_{{prefix_lc}}_pct_cmd_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_cmd_topic, 1);
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_pct_cmd_topic, 1);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.speed_param}}, &init);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic,
                                init.u8 > 0 ? "ON" : "OFF", 0, 1, true);
        char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)init.u8);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_pct_state_topic, buf, 0, 1, true);
    }
}
