{
    const esp_timer_create_args_t {{prefix_lc}}_args = {
        .callback = &{{prefix_lc}}_hlw_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_hlw",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, (uint64_t){{prefix}}_HLW_WINDOW_MS * 1000ULL));
}
