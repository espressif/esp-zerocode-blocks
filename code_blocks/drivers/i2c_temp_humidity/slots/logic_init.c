{
    const esp_timer_create_args_t {{prefix_lc}}_timer_args = {
        .callback = &{{prefix_lc}}_sensor_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_poll",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_timer_args, &{{prefix_lc}}_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, (uint64_t){{prefix}}_POLL_INTERVAL_MS * 1000ULL));
    /* Fire once immediately so the 'no sensor protocol' error is visible at boot,
     * not one poll period later. */
    {{prefix_lc}}_sensor_poll_cb(NULL);
    ESP_LOGW(TAG, "{{prefix_lc}}: I2C bus + poll timer up ({{cfg.poll_interval_ms}}ms) — NO sensor protocol, no readings published");
}
