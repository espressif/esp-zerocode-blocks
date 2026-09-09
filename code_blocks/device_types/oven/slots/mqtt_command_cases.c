if (tlen == (int)strlen(s_{{prefix_lc}}_setpoint_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_setpoint_cmd_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    app_driver_param_val_t v = { .i16 = (int16_t)(atof(pbuf) * 100.0) };
    app_driver_set_param({{cfg.setpoint_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_setpoint_state_topic, pbuf, 0, 1, true);
    return;
}
