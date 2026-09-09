{
    const esp_timer_create_args_t {{prefix_lc}}_timer_args = {
        .callback = &{{prefix_lc}}_input_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_in",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_timer_args, &{{prefix_lc}}_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, (uint64_t){{prefix}}_INPUT_POLL_MS * 1000ULL));
    {{prefix_lc}}_input_poll_cb(NULL);
}
