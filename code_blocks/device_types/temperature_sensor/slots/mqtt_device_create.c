{
    snprintf(s_{{prefix_lc}}_state_topic, sizeof(s_{{prefix_lc}}_state_topic),
             "zerocode/%s/{{prefix_lc}}/state", s_node_id);

    /* Retained HA discovery config. The state payload is a bare number in
     * °C — no value_template needed (jinja braces collide with the block
     * template engine anyway). */
    char disc_topic[80];
    snprintf(disc_topic, sizeof(disc_topic),
             "homeassistant/sensor/zc_%s_{{prefix_lc}}/config", s_node_id);
    char cfg_json[512];
    snprintf(cfg_json, sizeof(cfg_json),
             "{\"name\":\"{{cfg.label}}\",\"uniq_id\":\"zc_%s_{{prefix_lc}}\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},"
             "\"stat_t\":\"%s\",\"avty_t\":\"%s\","
             "\"dev_cla\":\"temperature\",\"unit_of_meas\":\"°C\",\"stat_cla\":\"measurement\"}",
             s_node_id, s_node_id, s_node_id, s_{{prefix_lc}}_state_topic, s_status_topic);
    esp_mqtt_client_publish(s_client, disc_topic, cfg_json, 0, 1, true);

    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.value_param}}, &init);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", (double)init.i16 / 100.0);
    /* A retained seed is a reading the broker keeps: only publish one once
     * the sensor has produced it — the zero the bus holds before that is not a
     * measurement. The normal publish path takes over on the first real value. */
    if (app_driver_param_seen({{cfg.value_param}})) {
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic, buf, 0, 1, true);
    }
    ESP_LOGI(TAG, "{{prefix_lc}}: HA temperature sensor '{{cfg.label}}' announced");
}
