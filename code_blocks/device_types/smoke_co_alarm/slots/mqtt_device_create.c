{
    snprintf(s_{{prefix_lc}}_smoke_topic, sizeof(s_{{prefix_lc}}_smoke_topic),
             "zerocode/%s/{{prefix_lc}}/smoke", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/binary_sensor/zc_%s_{{prefix_lc}}_smoke/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} smoke\",\"unique_id\":\"zc_%s_{{prefix_lc}}_smoke\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"smoke\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_smoke_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.smoke_state_param}}, &init);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_smoke_topic,
                                (init.u8 != 0) ? "ON" : "OFF", 0, 1, true);
    }
    snprintf(s_{{prefix_lc}}_co_topic, sizeof(s_{{prefix_lc}}_co_topic),
             "zerocode/%s/{{prefix_lc}}/co", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/binary_sensor/zc_%s_{{prefix_lc}}_co/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} co\",\"unique_id\":\"zc_%s_{{prefix_lc}}_co\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"carbon_monoxide\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_co_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.co_state_param}}, &init);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_co_topic,
                                (init.u8 != 0) ? "ON" : "OFF", 0, 1, true);
    }
}
