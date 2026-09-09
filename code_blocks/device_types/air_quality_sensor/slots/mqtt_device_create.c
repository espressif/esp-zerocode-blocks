{
    snprintf(s_{{prefix_lc}}_aqi_topic, sizeof(s_{{prefix_lc}}_aqi_topic),
             "zerocode/%s/{{prefix_lc}}/aqi", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}_aqi/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} aqi\",\"unique_id\":\"zc_%s_{{prefix_lc}}_aqi\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"aqi\",\"state_class\":\"measurement\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_aqi_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.air_quality_param}}, &init);
        char buf[24]; app_driver_param_val_t val = init;
        snprintf(buf, sizeof(buf), "%u", (unsigned)val.u8);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the sensor has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.air_quality_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_aqi_topic, buf, 0, 1, true);
        }
    }
{{#if cfg.co2_ppm_param}}
    snprintf(s_{{prefix_lc}}_co2_topic, sizeof(s_{{prefix_lc}}_co2_topic),
             "zerocode/%s/{{prefix_lc}}/co2", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}_co2/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} co2\",\"unique_id\":\"zc_%s_{{prefix_lc}}_co2\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"carbon_dioxide\",\"unit_of_measurement\":\"ppm\",\"state_class\":\"measurement\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_co2_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.co2_ppm_param}}, &init);
        char buf[24]; app_driver_param_val_t val = init;
        snprintf(buf, sizeof(buf), "%u", (unsigned)val.u16);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the sensor has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.co2_ppm_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_co2_topic, buf, 0, 1, true);
        }
    }
{{/if}}
{{#if cfg.pm25_param}}
    snprintf(s_{{prefix_lc}}_pm25_topic, sizeof(s_{{prefix_lc}}_pm25_topic),
             "zerocode/%s/{{prefix_lc}}/pm25", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}_pm25/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} pm25\",\"unique_id\":\"zc_%s_{{prefix_lc}}_pm25\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"device_class\":\"pm25\",\"unit_of_measurement\":\"µg/m³\",\"state_class\":\"measurement\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_pm25_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.pm25_param}}, &init);
        char buf[24]; app_driver_param_val_t val = init;
        snprintf(buf, sizeof(buf), "%u", (unsigned)val.u16);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the sensor has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.pm25_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_pm25_topic, buf, 0, 1, true);
        }
    }
{{/if}}
{{#if cfg.tvoc_param}}
    snprintf(s_{{prefix_lc}}_tvoc_topic, sizeof(s_{{prefix_lc}}_tvoc_topic),
             "zerocode/%s/{{prefix_lc}}/tvoc", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}_tvoc/config", s_node_id);
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}} tvoc\",\"unique_id\":\"zc_%s_{{prefix_lc}}_tvoc\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\",\"unit_of_measurement\":\"ppb\",\"state_class\":\"measurement\"}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_tvoc_topic);
        esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);
    }
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.tvoc_param}}, &init);
        char buf[24]; app_driver_param_val_t val = init;
        snprintf(buf, sizeof(buf), "%u", (unsigned)val.u16);
        /* A retained seed is a reading the broker keeps: only publish one once
         * the sensor has produced it — the zero the bus holds before that is not a
         * measurement. The normal publish path takes over on the first real value. */
        if (app_driver_param_seen({{cfg.tvoc_param}})) {
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_tvoc_topic, buf, 0, 1, true);
        }
    }
{{/if}}
}
