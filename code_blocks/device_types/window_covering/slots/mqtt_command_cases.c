if (tlen == (int)strlen(s_{{prefix_lc}}_setpos_topic) &&
    strncmp(topic, s_{{prefix_lc}}_setpos_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    int pct = atoi(pbuf); if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    app_driver_param_val_t v = { .u16 = (uint16_t)(pct * 100) };
    {{prefix}}_WC_COMMAND({{cfg.position_param}}, v, s_handle);
#if !{{prefix}}_WC_SPLIT
    zc_mqtt_publish(s_{{prefix_lc}}_pos_topic, pbuf, 0, 1, true);
#endif
    return;
}
if (tlen == (int)strlen(s_{{prefix_lc}}_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cmd_topic, tlen) == 0) {
    /* OPEN/CLOSE set the target; STOP fires stop_param when the instance has
     * one (otherwise the param model has no motion state to stop). With
     * current_param the position topic follows the measured position, not
     * the command. */
    if (dlen == 4 && strncmp(data, "OPEN", 4) == 0) {
        app_driver_param_val_t v = { .u16 = 10000 };
        {{prefix}}_WC_COMMAND({{cfg.position_param}}, v, s_handle);
#if !{{prefix}}_WC_SPLIT
        zc_mqtt_publish(s_{{prefix_lc}}_pos_topic, "100", 0, 1, true);
#endif
    } else if (dlen == 5 && strncmp(data, "CLOSE", 5) == 0) {
        app_driver_param_val_t v = { .u16 = 0 };
        {{prefix}}_WC_COMMAND({{cfg.position_param}}, v, s_handle);
#if !{{prefix}}_WC_SPLIT
        zc_mqtt_publish(s_{{prefix_lc}}_pos_topic, "0", 0, 1, true);
#endif
    }{{#if cfg.stop_param}} else if (dlen == 4 && strncmp(data, "STOP", 4) == 0) {
        app_driver_param_val_t v = { .u8 = 1 };
        app_driver_fire_event({{cfg.stop_param}}, v, s_handle);
    }{{/if}}
    return;
}
