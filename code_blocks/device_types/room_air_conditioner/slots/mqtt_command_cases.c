if (tlen == (int)strlen(s_{{prefix_lc}}_temp_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_temp_cmd_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    double t = atof(pbuf);
    if (t < 16) t = 16;
    if (t > 32) t = 32;
    app_driver_param_val_t v = { .i16 = (int16_t)(t * 100.0) };
    app_driver_set_param({{cfg.cool_setpoint_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_temp_state_topic, pbuf, 0, 1, true);
    return;
}
if (tlen == (int)strlen(s_{{prefix_lc}}_mode_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_mode_cmd_topic, tlen) == 0) {
    if (dlen == 3 && strncmp(data, "off", 3) == 0) {
        app_driver_param_val_t v = { .b = false };
        app_driver_set_param({{cfg.power_param}}, v, s_handle);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_mode_state_topic, "off", 0, 1, true);
    }
    if (dlen == 4 && strncmp(data, "cool", 4) == 0) {
        app_driver_param_val_t v = { .b = true };
        app_driver_set_param({{cfg.power_param}}, v, s_handle);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_mode_state_topic, "cool", 0, 1, true);
    }
    return;
}
