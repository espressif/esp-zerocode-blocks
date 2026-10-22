if (tlen == (int)strlen(s_{{prefix_lc}}_cooktime_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cooktime_cmd_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    /* Clamp exactly as the RainMaker binding does: an unclamped atoi let a
     * negative payload become a ~49-day u32 cook. Echo what was applied. */
    int secs = atoi(pbuf);
    secs = secs < 0 ? 0 : (secs > 3600 ? 3600 : secs);
    app_driver_param_val_t v = { .u32 = (uint32_t)secs };
    app_driver_set_param({{cfg.cook_time_param}}, v, s_handle);
    snprintf(pbuf, sizeof(pbuf), "%d", secs);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_cooktime_state_topic, pbuf, 0, 1, true);
    return;
}
if (tlen == (int)strlen(s_{{prefix_lc}}_power_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_power_cmd_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    /* 10-100 %, as the RainMaker binding clamps (block.yml). */
    int level = atoi(pbuf);
    level = level < 10 ? 10 : (level > 100 ? 100 : level);
    app_driver_param_val_t v = { .u8 = (uint8_t)level };
    app_driver_set_param({{cfg.power_level_param}}, v, s_handle);
    snprintf(pbuf, sizeof(pbuf), "%d", level);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_power_state_topic, pbuf, 0, 1, true);
    return;
}
