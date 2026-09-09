if (tlen == (int)strlen(s_{{prefix_lc}}_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cmd_topic, tlen) == 0) {
    bool lock = (dlen == 4 && strncmp(data, "LOCK", 4) == 0);
    app_driver_param_val_t v = { .b = lock };
    app_driver_set_param({{cfg.locked_param}}, v, s_handle);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic,
                            lock ? "LOCKED" : "UNLOCKED", 0, 1, true);
    return;
}
