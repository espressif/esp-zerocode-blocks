if (tlen == (int)strlen(s_{{prefix_lc}}_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cmd_topic, tlen) == 0) {
    bool on = (dlen == 2 && strncmp(data, "ON", 2) == 0);
    app_driver_param_val_t v = { .b = on };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic, on ? "ON" : "OFF", 0, 1, true);
    return;
}
if (tlen == (int)strlen(s_{{prefix_lc}}_bri_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_bri_cmd_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    int b = atoi(pbuf); if (b < 1) b = 1; if (b > 254) b = 254;
    app_driver_param_val_t v = { .u8 = (uint8_t)b };
    app_driver_set_param({{cfg.brightness_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_bri_state_topic, pbuf, 0, 1, true);
    return;
}
