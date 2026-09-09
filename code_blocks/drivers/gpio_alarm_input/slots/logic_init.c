{
    const esp_timer_create_args_t {{prefix_lc}}_alarm_args = {
        .callback = &{{prefix_lc}}_alarm_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_alarm",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_alarm_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_alarm_args, &{{prefix_lc}}_alarm_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_alarm_timer, (uint64_t){{prefix}}_ALARM_POLL_MS * 1000ULL));
    {{prefix_lc}}_alarm_poll_cb(NULL);
}
