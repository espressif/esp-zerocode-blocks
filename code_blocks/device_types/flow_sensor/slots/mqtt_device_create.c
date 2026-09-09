{
    snprintf(s_{{prefix_lc}}_flow_topic, sizeof(s_{{prefix_lc}}_flow_topic),
             "zerocode/%s/{{prefix_lc}}/flow", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}_flow/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}_flow\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"unit_of_measurement\":\"m³/h\",\"state_class\":\"measurement\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_flow_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.value_param}}, &init);
        char buf[24]; app_driver_param_val_t val = init;
        snprintf(buf, sizeof(buf), "%.1f", (double)val.u16 / 10.0);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the sensor has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.value_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_flow_topic, buf, 0, 1, true);
        }
    }
}
