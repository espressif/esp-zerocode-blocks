if (tlen == (int)strlen(s_{{prefix_lc}}_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cmd_topic, tlen) == 0) {
    for (int i = 0; i < 4; i++) {
        if (dlen == (int)strlen(s_{{prefix_lc}}_options[i]) &&
            strncmp(data, s_{{prefix_lc}}_options[i], dlen) == 0) {
            app_driver_param_val_t v = { .u8 = (uint8_t)i };
            app_driver_set_param({{cfg.dryness_param}}, v, s_handle);
            esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic,
                                    s_{{prefix_lc}}_options[i], 0, 1, true);
            break;
        }
    }
    return;
}
