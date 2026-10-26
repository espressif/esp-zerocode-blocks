if (tlen == (int)strlen(s_{{prefix_lc}}_temp_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_temp_cmd_topic, tlen) == 0) {
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    double t = atof(pbuf);
    /* The discovery's min_temp / max_temp / temp_step: clamp, then snap to
     * the nearest 0.5 °C. The state topic echoes the value APPLIED, never the
     * payload — HA keeps the retained state, and a raw "40" or "21.3" there
     * would be a target the device is not holding. */
    if (t < 5) t = 5;
    if (t > 35) t = 35;
    long halves = (long)(t * 2.0 + 0.5);
    app_driver_param_val_t v = { .i16 = (int16_t)(halves * 50) };
    app_driver_set_param({{cfg.heat_setpoint_param}}, v, s_handle);
    char applied[16];
    snprintf(applied, sizeof(applied), "%.2f", (double)v.i16 / 100.0);
    zc_mqtt_publish(s_{{prefix_lc}}_temp_state_topic, applied, 0, 1, true);
    return;
}
if (tlen == (int)strlen(s_{{prefix_lc}}_mode_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_mode_cmd_topic, tlen) == 0) {
    if (dlen == 3 && strncmp(data, "off", 3) == 0) {
        app_driver_param_val_t v = { .u8 = 0 };
        app_driver_set_param({{cfg.system_mode_param}}, v, s_handle);
        zc_mqtt_publish(s_{{prefix_lc}}_mode_state_topic, "off", 0, 1, true);
    }
    if (dlen == 4 && strncmp(data, "heat", 4) == 0) {
        app_driver_param_val_t v = { .u8 = 4 };
        app_driver_set_param({{cfg.system_mode_param}}, v, s_handle);
        zc_mqtt_publish(s_{{prefix_lc}}_mode_state_topic, "heat", 0, 1, true);
    }
{{#if cfg.heating_param}}
    /* The bus does not hand this binding its own write back, so the action
     * that follows the new mode is published here. */
    {{prefix_lc}}_publish_action();
{{/if}}
    return;
}
