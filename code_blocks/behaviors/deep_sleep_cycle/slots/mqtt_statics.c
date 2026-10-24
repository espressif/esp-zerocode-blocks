/* {{prefix_lc}} (deep_sleep_cycle): tells the sleep logic when the state is
 * delivered, and signs off cleanly before sleeping. */
static volatile bool s_{{prefix_lc}}_dsc_announced = false;

extern "C" int {{prefix_lc}}_dsc_net_state(void)
{
    if (!app_mqtt_configured()) return {{prefix}}_DSC_NET_UNCONFIGURED;
    if (!s_connected || s_client == NULL || !s_{{prefix_lc}}_dsc_announced) return {{prefix}}_DSC_NET_PENDING;
    /* QoS 1 publishes stay in the outbox until the broker acknowledges them. */
    return esp_mqtt_client_get_outbox_size(s_client) == 0 ? {{prefix}}_DSC_NET_REPORTED
                                                          : {{prefix}}_DSC_NET_PENDING;
}

extern "C" void {{prefix_lc}}_dsc_net_quiet(void)
{
    s_{{prefix_lc}}_dsc_announced = false;
    if (s_client == NULL || !s_connected) return;
    /* stop() sends a clean DISCONNECT: the broker drops the Last Will, so the
     * retained "online" status stands while the device sleeps. */
    esp_mqtt_client_stop(s_client);
}
