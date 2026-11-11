{
    snprintf(s_{{prefix_lc}}_state_topic, sizeof(s_{{prefix_lc}}_state_topic),
             "zerocode/%s/{{prefix_lc}}/state", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/binary_sensor/zc_%s_{{prefix_lc}}/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\"{{#if cfg.device_class}},\"device_class\":\"{{cfg.device_class}}\"{{/if}}}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_state_topic);
        zc_mqtt_publish(disc_topic, cfg_json, 0, 1, true);
    }
    /* Retained state only once the flag has been set: until then HA shows
     * unknown, not a false "off". */
    if (app_driver_param_seen({{cfg.value_param}})) {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.value_param}}, &init);
        zc_mqtt_publish(s_{{prefix_lc}}_state_topic, init.b ? "ON" : "OFF", 0, 1, true);
    }
}
