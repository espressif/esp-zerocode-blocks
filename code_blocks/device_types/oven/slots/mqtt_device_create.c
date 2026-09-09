{
    snprintf(s_{{prefix_lc}}_setpoint_state_topic, sizeof(s_{{prefix_lc}}_setpoint_state_topic),
             "zerocode/%s/{{prefix_lc}}/setpoint", s_node_id);
    snprintf(s_{{prefix_lc}}_setpoint_cmd_topic, sizeof(s_{{prefix_lc}}_setpoint_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/setpoint/set", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/number/zc_%s_{{prefix_lc}}_setpoint/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}_setpoint\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"min\":0,\"max\":260,\"step\":5,\"unit_of_measurement\":\"°C\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_setpoint_state_topic,
                 s_{{prefix_lc}}_setpoint_cmd_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_setpoint_cmd_topic, 1);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.setpoint_param}}, &init);
        char buf[16]; snprintf(buf, sizeof(buf), "%.2f", (double)init.i16 / 100.0);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_setpoint_state_topic, buf, 0, 1, true);
    }
}
