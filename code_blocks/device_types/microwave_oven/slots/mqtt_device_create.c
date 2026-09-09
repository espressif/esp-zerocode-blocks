{
    snprintf(s_{{prefix_lc}}_cooktime_state_topic, sizeof(s_{{prefix_lc}}_cooktime_state_topic),
             "zerocode/%s/{{prefix_lc}}/cooktime", s_node_id);
    snprintf(s_{{prefix_lc}}_cooktime_cmd_topic, sizeof(s_{{prefix_lc}}_cooktime_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/cooktime/set", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/number/zc_%s_{{prefix_lc}}_cooktime/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} cooktime\",\"unique_id\":\"zc_%s_{{prefix_lc}}_cooktime\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"min\":0,\"max\":3600,\"step\":30,\"unit_of_measurement\":\"s\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_cooktime_state_topic,
                 s_{{prefix_lc}}_cooktime_cmd_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_cooktime_cmd_topic, 1);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.cook_time_param}}, &init);
        char buf[16]; snprintf(buf, sizeof(buf), "%u", (unsigned)init.u32);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_cooktime_state_topic, buf, 0, 1, true);
    }
    snprintf(s_{{prefix_lc}}_power_state_topic, sizeof(s_{{prefix_lc}}_power_state_topic),
             "zerocode/%s/{{prefix_lc}}/power", s_node_id);
    snprintf(s_{{prefix_lc}}_power_cmd_topic, sizeof(s_{{prefix_lc}}_power_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/power/set", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/number/zc_%s_{{prefix_lc}}_power/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} power\",\"unique_id\":\"zc_%s_{{prefix_lc}}_power\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"min\":10,\"max\":100,\"step\":10,\"unit_of_measurement\":\"%%\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_power_state_topic,
                 s_{{prefix_lc}}_power_cmd_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_power_cmd_topic, 1);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.power_level_param}}, &init);
        char buf[16]; snprintf(buf, sizeof(buf), "%u", (unsigned)init.u8);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_power_state_topic, buf, 0, 1, true);
    }
}
