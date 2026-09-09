{
#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED
    const esp_timer_create_args_t {{prefix_lc}}_args = {
        .callback = &{{prefix_lc}}_recov_check,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_recov",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &s_{{prefix_lc}}_recov_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_{{prefix_lc}}_recov_timer, 30000000ULL)); /* 30 s */
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, {{prefix_lc}}_wifi_event_cb, NULL));
#endif
}
