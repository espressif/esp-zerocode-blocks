{
    snprintf(s_{{prefix_lc}}_freeze_topic, sizeof(s_{{prefix_lc}}_freeze_topic),
             "zerocode/%s/{{prefix_lc}}/freeze", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/binary_sensor/zc_%s_{{prefix_lc}}_freeze/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}_freeze\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"cold\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_freeze_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.freeze_param}}, &init);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_freeze_topic,
                                (init.b) ? "ON" : "OFF", 0, 1, true);
    }
}
