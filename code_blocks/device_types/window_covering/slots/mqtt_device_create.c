{
    snprintf(s_{{prefix_lc}}_pos_topic, sizeof(s_{{prefix_lc}}_pos_topic),
             "zerocode/%s/{{prefix_lc}}/position", s_node_id);
    snprintf(s_{{prefix_lc}}_setpos_topic, sizeof(s_{{prefix_lc}}_setpos_topic),
             "zerocode/%s/{{prefix_lc}}/position/set", s_node_id);
    snprintf(s_{{prefix_lc}}_cmd_topic, sizeof(s_{{prefix_lc}}_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/set", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/cover/zc_%s_{{prefix_lc}}/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"position_topic\":\"%s\",\"set_position_topic\":\"%s\",\"command_topic\":\"%s\",\"position_open\":100,\"position_closed\":0,\"device_class\":\"blind\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_pos_topic,
                 s_{{prefix_lc}}_setpos_topic,
                 s_{{prefix_lc}}_cmd_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_setpos_topic, 1);
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_cmd_topic, 1);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.position_param}}, &init);
        char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)(init.u16 / 100));
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_pos_topic, buf, 0, 1, true);
    }
}
