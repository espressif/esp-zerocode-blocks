{{#if cfg.status_param}}
if (param_id == {{cfg.status_param}}) {
    /* No valid reading: Home Assistant shows the sensor as unknown ("None"),
       not the last value. Valid again: republish the current reading — an
       unchanged value sends no notification of its own. */
    if (!val.b) {
        zc_mqtt_publish(s_{{prefix_lc}}_moisture_topic, "None", 0, 1, true);
    } else if (app_driver_param_seen({{cfg.value_param}})) {
        app_driver_param_val_t cur = {};
        app_driver_get_param({{cfg.value_param}}, &cur);
        {{prefix_lc}}_mqtt_publish_reading(cur);
    }
}
{{/if}}
