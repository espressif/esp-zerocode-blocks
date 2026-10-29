{
    snprintf(s_{{prefix_lc}}_state_topic, sizeof(s_{{prefix_lc}}_state_topic),
             "zerocode/%s/{{prefix_lc}}/state", s_node_id);
    {
        char disc_topic[96];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/sensor/zc_%s_{{prefix_lc}}/config", s_node_id);
        /* The unit is an argument, never part of the format: "%" is a common one. */
        char cfg_json[960];
        snprintf(cfg_json, sizeof(cfg_json),
                 "{\"name\":\"{{cfg.label}}\",\"unique_id\":\"zc_%s_{{prefix_lc}}\",\"availability_topic\":\"%s\",\"device\":{\"identifiers\":[\"zc_%s\"],\"name\":\"ZeroCode %s\",\"manufacturer\":\"ZeroCode AI\"},\"state_topic\":\"%s\"{{#if cfg.unit}},\"unit_of_measurement\":\"%s\"{{/if}}{{#if cfg.device_class}},\"device_class\":\"{{cfg.device_class}}\"{{/if}}{{#if cfg.state_class}},\"state_class\":\"{{cfg.state_class}}\"{{/if}}}",
                 s_node_id,
                 s_status_topic,
                 s_node_id,
                 s_node_id,
                 s_{{prefix_lc}}_state_topic{{#if cfg.unit}},
                 "{{cfg.unit}}"{{/if}});
        zc_mqtt_publish(disc_topic, cfg_json, 0, 1, true);
    }
    /* Retained state only once the value has been set: the zero the bus holds
     * before that is not a reading. */
    if (app_driver_param_seen({{cfg.value_param}})) {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.value_param}}, &init);
        /* A sensor marked invalid seeds "None" (unknown), not a stale value. */
        if (!{{prefix_lc}}_reading_valid()) zc_mqtt_publish(s_{{prefix_lc}}_state_topic, "None", 0, 1, true);
        else {{prefix_lc}}_mqtt_publish_value(init.i16);
    }
}
