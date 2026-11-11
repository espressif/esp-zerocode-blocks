{
    const esp_timer_create_args_t {{prefix_lc}}_cd_args = {
        .callback = &{{prefix_lc}}_cd_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_cd",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_cd_args, &s_{{prefix_lc}}_cd_tick));
    s_{{prefix_lc}}_cd_handle = app_driver_register_solution("{{prefix_lc}}_countdown", {{prefix_lc}}_cd_cb, NULL);
    /* Not persisted: whatever minutes_param says at boot, no countdown runs. */
    app_driver_param_val_t {{prefix_lc}}_cd_zero = { .u16 = 0 };
    app_driver_set_param({{cfg.minutes_param}}, {{prefix_lc}}_cd_zero, s_{{prefix_lc}}_cd_handle);
    {{prefix_lc}}_cd_publish_remaining(0);
}
