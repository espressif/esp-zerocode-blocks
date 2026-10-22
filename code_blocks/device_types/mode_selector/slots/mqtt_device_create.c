{
    snprintf(s_{{prefix_lc}}_state_topic, sizeof(s_{{prefix_lc}}_state_topic),
             "zerocode/%s/{{prefix_lc}}/state", s_node_id);
    snprintf(s_{{prefix_lc}}_cmd_topic, sizeof(s_{{prefix_lc}}_cmd_topic),
             "zerocode/%s/{{prefix_lc}}/set", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/select/zc_%s_{{prefix_lc}}/config", s_node_id);
        /* static: the options list grows with mode_count, and this runs on
         * the MQTT task's stack. */
        static char options[16 * ZC_{{prefix_lc}}_MODE_COUNT + 2];
        size_t olen = 0;
        options[0] = 0;
        for (int i = 0; i < ZC_{{prefix_lc}}_MODE_COUNT && olen < sizeof(options); i++) {
            olen += snprintf(options + olen, sizeof(options) - olen, "%s\"Mode %d\"", i ? "," : "", i + 1);
        }
        static char cfg_json[640 + sizeof(options)];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"command_topic\":\"%s\",\"options\":[%s]}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_state_topic,
                 s_{{prefix_lc}}_cmd_topic,
                 options);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    esp_mqtt_client_subscribe(s_client, s_{{prefix_lc}}_cmd_topic, 1);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.mode_param}}, &init);
        {{prefix_lc}}_publish_mode(init.u8);
    }
}
