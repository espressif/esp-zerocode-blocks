if (tlen == (int)strlen(s_{{prefix_lc}}_cooktime_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cooktime_cmd_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    app_driver_param_val_t v = { .u32 = (uint32_t)atoi(pbuf) };
    app_driver_set_param({{cfg.cook_time_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_cooktime_state_topic, pbuf, 0, 1, true);
    return;
}
if (tlen == (int)strlen(s_{{prefix_lc}}_power_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_power_cmd_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    app_driver_param_val_t v = { .u8 = (uint8_t)atoi(pbuf) };
    app_driver_set_param({{cfg.power_level_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_power_state_topic, pbuf, 0, 1, true);
    return;
}
