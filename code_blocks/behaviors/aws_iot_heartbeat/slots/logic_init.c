{
    /* Subscribing and topic-building happen on ZC_AWS_EVENT_CONNECTED, not
     * here: the thing name is not final until the device has been onboarded. */
    esp_err_t {{prefix_lc}}_err = esp_event_handler_register(
        ZC_AWS_EVENT, ZC_AWS_EVENT_CONNECTED, {{prefix_lc}}_on_connected, NULL);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* Worth shouting about: a failed registration is a device that connects
         * happily and then receives nothing, with no other symptom. */
        ESP_LOGE(TAG, "{{prefix_lc}}: cannot hook ZC_AWS_EVENT_CONNECTED: %s",
                 esp_err_to_name({{prefix_lc}}_err));
    }

    esp_timer_create_args_t {{prefix_lc}}_args = {};
    {{prefix_lc}}_args.callback = {{prefix_lc}}_beat;
    {{prefix_lc}}_args.name = "{{prefix_lc}}_beat";
    esp_timer_handle_t {{prefix_lc}}_timer = NULL;
    if (esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer) == ESP_OK) {
        esp_timer_start_periodic({{prefix_lc}}_timer,
                                 (uint64_t){{prefix}}_PERIOD_S * 1000000ULL);
        ESP_LOGI(TAG, "{{prefix_lc}}: heartbeat timer every %ds", {{prefix}}_PERIOD_S);
    }
}
