if (tlen == (int)strlen(s_{{prefix_lc}}_temp_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_temp_cmd_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    double t = atof(pbuf);
    if (t < 5) t = 5;
    if (t > 35) t = 35;
    app_driver_param_val_t v = { .i16 = (int16_t)(t * 100.0) };
    app_driver_set_param({{cfg.heat_setpoint_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_temp_state_topic, pbuf, 0, 1, true);
    return;
}
if (tlen == (int)strlen(s_{{prefix_lc}}_mode_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_mode_cmd_topic, tlen) == 0) {
    if (dlen == 3 && strncmp(data, "off", 3) == 0) {
        app_driver_param_val_t v = { .u8 = 0 };
        app_driver_set_param({{cfg.system_mode_param}}, v, s_handle);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_mode_state_topic, "off", 0, 1, true);
    }
    if (dlen == 4 && strncmp(data, "heat", 4) == 0) {
        app_driver_param_val_t v = { .u8 = 4 };
        app_driver_set_param({{cfg.system_mode_param}}, v, s_handle);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_mode_state_topic, "heat", 0, 1, true);
    }
    return;
}
