if (tlen == (int)strlen(s_{{prefix_lc}}_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cmd_topic, tlen) == 0) {
    bool on = (dlen == 2 && strncmp(data, "ON", 2) == 0);
    app_driver_param_val_t v = { .b = on };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    /* The bus skips notifying the source, so echo the state ourselves. */
    zc_mqtt_publish(s_{{prefix_lc}}_state_topic,
                    on ? "ON" : "OFF", 0, 1, true);
    return;
}
