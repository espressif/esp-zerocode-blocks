{
    snprintf(s_{{prefix_lc}}_moisture_topic, sizeof(s_{{prefix_lc}}_moisture_topic),
             "zerocode/%s/{{prefix_lc}}/moisture", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}_moisture/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}_moisture\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"moisture\",\"unit_of_measurement\":\"%%\",\"state_class\":\"measurement\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_moisture_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.value_param}}, &init);
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", (unsigned)init.u8);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the probe has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.value_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_moisture_topic, buf, 0, 1, true);
        }
    }
}
