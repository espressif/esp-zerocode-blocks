{
    snprintf(s_{{prefix_lc}}_power_topic, sizeof(s_{{prefix_lc}}_power_topic),
             "zerocode/%s/{{prefix_lc}}/power", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}_power/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} power\",\"unique_id\":\"zc_%s_{{prefix_lc}}_power\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"power\",\"unit_of_measurement\":\"W\",\"state_class\":\"measurement\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_power_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.power_param}}, &init);
        char buf[24]; app_driver_param_val_t val = init;
        snprintf(buf, sizeof(buf), "%.2f", (double)val.u32 / 1000.0);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the sensor has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.power_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_power_topic, buf, 0, 1, true);
        }
    }
    snprintf(s_{{prefix_lc}}_energy_topic, sizeof(s_{{prefix_lc}}_energy_topic),
             "zerocode/%s/{{prefix_lc}}/energy", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}_energy/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} energy\",\"unique_id\":\"zc_%s_{{prefix_lc}}_energy\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"energy\",\"unit_of_measurement\":\"Wh\",\"state_class\":\"total_increasing\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_energy_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.energy_param}}, &init);
        char buf[24]; app_driver_param_val_t val = init;
        snprintf(buf, sizeof(buf), "%u", (unsigned)val.u32);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the sensor has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.energy_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_energy_topic, buf, 0, 1, true);
        }
    }
}
