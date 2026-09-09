if (tlen == (int)strlen(s_{{prefix_lc}}_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cmd_topic, tlen) == 0) {
    bool on = (dlen == 2 && strncmp(data, "ON", 2) == 0);
    app_driver_param_val_t v = { .u8 = (uint8_t)(on ? 100 : 0) };
    app_driver_set_param({{cfg.speed_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic, on ? "ON" : "OFF", 0, 1, true);
    return;
}
if (tlen == (int)strlen(s_{{prefix_lc}}_pct_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_pct_cmd_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    int pct = atoi(pbuf); if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    app_driver_param_val_t v = { .u8 = (uint8_t)pct };
    app_driver_set_param({{cfg.speed_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_pct_state_topic, pbuf, 0, 1, true);
    return;
}
