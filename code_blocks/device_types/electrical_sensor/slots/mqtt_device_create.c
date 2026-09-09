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
        app_driver_get_param({{cfg.active_power_param}}, &init);
        char buf[24]; app_driver_param_val_t val = init;
        snprintf(buf, sizeof(buf), "%.2f", (double)val.u32 / 1000.0);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the sensor has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.active_power_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_power_topic, buf, 0, 1, true);
        }
    }
{{#if cfg.voltage_param}}
    snprintf(s_{{prefix_lc}}_voltage_topic, sizeof(s_{{prefix_lc}}_voltage_topic),
             "zerocode/%s/{{prefix_lc}}/voltage", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}_voltage/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} voltage\",\"unique_id\":\"zc_%s_{{prefix_lc}}_voltage\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"voltage\",\"unit_of_measurement\":\"V\",\"state_class\":\"measurement\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_voltage_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.voltage_param}}, &init);
        char buf[24]; app_driver_param_val_t val = init;
        snprintf(buf, sizeof(buf), "%.2f", (double)val.u32 / 1000.0);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the sensor has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.voltage_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_voltage_topic, buf, 0, 1, true);
        }
    }
{{/if}}
{{#if cfg.current_param}}
    snprintf(s_{{prefix_lc}}_current_topic, sizeof(s_{{prefix_lc}}_current_topic),
             "zerocode/%s/{{prefix_lc}}/current", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}_current/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} current\",\"unique_id\":\"zc_%s_{{prefix_lc}}_current\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"current\",\"unit_of_measurement\":\"A\",\"state_class\":\"measurement\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_current_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.current_param}}, &init);
        char buf[24]; app_driver_param_val_t val = init;
        snprintf(buf, sizeof(buf), "%.2f", (double)val.u32 / 1000.0);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the sensor has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.current_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_current_topic, buf, 0, 1, true);
        }
    }
{{/if}}
}
