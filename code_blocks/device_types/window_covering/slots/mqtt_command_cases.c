if (tlen == (int)strlen(s_{{prefix_lc}}_setpos_topic) &&
    strncmp(topic, s_{{prefix_lc}}_setpos_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    int pct = atoi(pbuf); if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    app_driver_param_val_t v = { .u16 = (uint16_t)(pct * 100) };
    app_driver_set_param({{cfg.position_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_pos_topic, pbuf, 0, 1, true);
    return;
}
if (tlen == (int)strlen(s_{{prefix_lc}}_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cmd_topic, tlen) == 0) {
    /* OPEN/CLOSE; STOP is a no-op (the param model has no motion state) */
    if (dlen == 4 && strncmp(data, "OPEN", 4) == 0) {
        app_driver_param_val_t v = { .u16 = 10000 };
        app_driver_set_param({{cfg.position_param}}, v, s_handle);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_pos_topic, "100", 0, 1, true);
    } else if (dlen == 5 && strncmp(data, "CLOSE", 5) == 0) {
        app_driver_param_val_t v = { .u16 = 0 };
        app_driver_set_param({{cfg.position_param}}, v, s_handle);
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_pos_topic, "0", 0, 1, true);
    }
    return;
}
