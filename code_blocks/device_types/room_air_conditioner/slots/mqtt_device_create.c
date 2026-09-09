{
    snprintf(s_{{prefix_lc}}_curr_topic, sizeof(s_{{prefix_lc}}_curr_topic),
             "zerocode/%s/{{prefix_lc}}/curr", s_node_id);
    snprintf(s_{{prefix_lc}}_temp_state_topic, sizeof(s_{{prefix_lc}}_temp_state_topic),
             "zerocode/%s/{{prefix_lc}}/temp/state", s_node_id);
    snprintf(s_{{prefix_lc}}_temp_cmd_topic, sizeof(s_{{prefix_lc}}_temp_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/temp/cmd", s_node_id);
    snprintf(s_{{prefix_lc}}_mode_state_topic, sizeof(s_{{prefix_lc}}_mode_state_topic),
             "zerocode/%s/{{prefix_lc}}/mode/state", s_node_id);
    snprintf(s_{{prefix_lc}}_mode_cmd_topic, sizeof(s_{{prefix_lc}}_mode_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/mode/cmd", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/climate/zc_%s_{{prefix_lc}}/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"current_temperature_topic\":\"%s\",\"temperature_state_topic\":\"%s\",\"temperature_command_topic\":\"%s\",\"mode_state_topic\":\"%s\",\"mode_command_topic\":\"%s\",\"modes\":[\"off\",\"cool\"],\"min_temp\":16,\"max_temp\":32,\"temp_step\":0.5,\"temperature_unit\":\"C\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_curr_topic,
                 s_{{prefix_lc}}_temp_state_topic,
                 s_{{prefix_lc}}_temp_cmd_topic,
                 s_{{prefix_lc}}_mode_state_topic,
                 s_{{prefix_lc}}_mode_cmd_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_temp_cmd_topic, 1);
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_mode_cmd_topic, 1);
    {
        app_driver_param_val_t init = {};
        char buf[16];
        app_driver_get_param({{cfg.cool_setpoint_param}}, &init);
        snprintf(buf, sizeof(buf), "%.2f", (double)init.i16 / 100.0);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_temp_state_topic, buf, 0, 1, true);
    }
}
