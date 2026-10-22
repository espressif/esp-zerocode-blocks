case {{cfg.position_param}}: {
    /* Each nonzero notification = one press (not retained — events, not
     * state). A position of 0 carries no press (the position_param contract),
     * as in the Matter binding. */
    if (val.u8 != 0) {
        esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_trigger_topic, "press", 0, 1, false);
    }
    return;
}
